// ============================================================
//  Schlüsselwort-Erkennung: Merkmale + kleines Netz (siehe kws.h)
//
//  Dateiformat kws_de.bin (Little Endian, geschrieben von
//  Sprachmodell/kws_train.py, Kopf und Felder dort identisch):
//   "KWS1"
//   u16 version(1) rate win hop nfft mels frames infer_hops cmn_frames
//   f32 fmin fmax threshold wake_threshold
//   f32 mean[mels] istd[mels]
//   u8  Klassen, je u8 Länge + Name
//   u8  Schichten, je u8 Typ:
//       1 CONV  u16 oc, u8 kh kw sh sw pad act, f32 w[oc][kh][kw][ic], f32 b[oc]
//       2 DW    u8 kh kw sh sw pad act, f32 w[kh][kw][c], f32 b[c]
//       3 GAP   (Mittelwert über Zeit und Frequenz)
//       4 DENSE u16 out, u8 act, f32 w[out][in], f32 b[out]
//   u16 Länge + Infotext
//  pad: 0 = valid, 1 = same (wie TensorFlow), act: 0 = keine, 1 = ReLU.
//  Tensoren liegen als [Zeit][Mel][Kanal] im Speicher.
// ============================================================
#include "kws.h"
#include "storage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
static void *kws_alloc(size_t n) {  // große Puffer ins PSRAM
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}
static uint32_t kws_us() { return (uint32_t)micros(); }
#else
#include <chrono>
static void *kws_alloc(size_t n) { return malloc(n); }
static uint32_t kws_us() {
  return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}
#endif

#define NFFT_MAX 512
#define MAX_LAYERS 24
#define SMOOTH 3  // so viele Auswertungen werden gemittelt

enum { L_CONV = 1, L_DW = 2, L_GAP = 3, L_DENSE = 4 };

typedef struct {
  uint8_t type, kh, kw, sh, sw, pad, act;
  int ih, iw, ic, oh, ow, oc, pt, pl;
  float *w, *b;
} layer_t;

// ---- Modell ----
static bool s_ready = false;
static char s_err[64] = "kein Modell geladen";
static char s_info[96] = "";
static int s_rate, s_win, s_hop, s_nfft, s_mels, s_frames, s_infer_hops, s_cmn_frames;
static float s_fmin, s_fmax, s_thr, s_wake_thr;
static float *s_mean, *s_istd;
static int s_ncls;
static char s_cls[KWS_MAX_CLASSES][24];
static layer_t s_lay[MAX_LAYERS];
static int s_nlay;
static float *s_pool;      // alle Gewichte (ausgerichtet kopiert)
static float *s_act[2];    // Zwischenergebnisse, abwechselnd
static int s_act_len;

// ---- Merkmale ----
// Tabellen und Arbeitsspeicher der FFT: beim Laden vom Heap (interner RAM ist knapp)
static float *s_hann, *s_cos, *s_sin, *s_re, *s_im, *s_pw;
static uint16_t s_rev[NFFT_MAX];
static int s_mel_start[64], s_mel_len[64];
static float *s_mel_w;     // Gewichte aller Bänder hintereinander
static float *s_buf;       // letzte win Abtastwerte (nach Hochpass)
static int s_pend;         // neue Werte seit dem letzten Rahmen
static float s_hp_x1, s_hp_y1;
static float *s_cmn;       // gleitender Mittelwert je Band
static bool s_cmn_init;
static float *s_ring;      // frames x mels, normiert
static int s_ring_pos, s_ring_fill, s_since_infer;
static float s_probs[KWS_MAX_CLASSES], s_hist[SMOOTH][KWS_MAX_CLASSES];
static int s_hist_n, s_hist_pos;
static uint32_t s_infer_us;
static float *s_input;     // frames x mels für das Netz

const char *kws_error() { return s_err; }
const char *kws_info() { return s_info; }
bool kws_ready() { return s_ready; }
int kws_num_classes() { return s_ready ? s_ncls : 0; }
const char *kws_class(int i) { return (i >= 0 && i < s_ncls) ? s_cls[i] : ""; }
float kws_threshold() { return s_thr; }
float kws_wake_threshold() { return s_wake_thr; }
const float *kws_probs() { return s_probs; }
uint32_t kws_infer_us() { return s_infer_us; }
int kws_frames() { return s_frames; }
int kws_mels() { return s_mels; }

int kws_class_index(const char *name) {
  for (int i = 0; i < s_ncls; i++)
    if (strcmp(s_cls[i], name) == 0) return i;
  return -1;
}

// ------------------------------------------------------------
//  Lesen des Modells
// ------------------------------------------------------------
typedef struct {
  const uint8_t *p, *end;
  bool bad;
} rd_t;
static bool rd_has(rd_t *r, int n) {
  if (r->bad || r->p + n > r->end) {
    r->bad = true;
    return false;
  }
  return true;
}
static uint8_t rd_u8(rd_t *r) { return rd_has(r, 1) ? *r->p++ : 0; }
static uint16_t rd_u16(rd_t *r) {
  if (!rd_has(r, 2)) return 0;
  uint16_t v = (uint16_t)(r->p[0] | (r->p[1] << 8));
  r->p += 2;
  return v;
}
static float rd_f32(rd_t *r) {
  float v = 0;
  if (rd_has(r, 4)) {
    memcpy(&v, r->p, 4);  // nicht ausgerichtet: kopieren
    r->p += 4;
  }
  return v;
}
static void rd_floats(rd_t *r, float *dst, int n) {
  if (!rd_has(r, n * 4)) return;
  memcpy(dst, r->p, (size_t)n * 4);
  r->p += n * 4;
}

static void fail(const char *msg) {
  snprintf(s_err, sizeof(s_err), "%s", msg);
  s_ready = false;
}

void kws_unload() {
  s_ready = false;
  void *bufs[] = { s_pool, s_act[0], s_act[1], s_mean, s_istd, s_mel_w, s_buf, s_cmn, s_ring, s_input,
                   s_hann, s_cos, s_sin, s_re, s_im, s_pw };
  for (void *b : bufs) free(b);
  s_pool = s_act[0] = s_act[1] = s_mean = s_istd = s_mel_w = s_buf = s_cmn = s_ring = s_input = NULL;
  s_hann = s_cos = s_sin = s_re = s_im = s_pw = NULL;
  s_nlay = s_ncls = 0;
  snprintf(s_err, sizeof(s_err), "kein Modell geladen");
}

// Ausgabegröße wie TensorFlow (same: aufrunden, Rand halb vorn, halb hinten)
static void out_dim(int in, int k, int s, int pad, int *out, int *before) {
  if (pad) {
    *out = (in + s - 1) / s;
    int total = (*out - 1) * s + k - in;
    if (total < 0) total = 0;
    *before = total / 2;
  } else {
    *out = (in - k) / s + 1;
    *before = 0;
  }
}

static float hz_to_mel(float f) { return 2595.0f * log10f(1.0f + f / 700.0f); }
static float mel_to_hz(float m) { return 700.0f * (powf(10.0f, m / 2595.0f) - 1.0f); }

static bool setup_frontend() {
  if (s_nfft > NFFT_MAX || (s_nfft & (s_nfft - 1)) || s_win > s_nfft || s_hop > s_win || s_mels > 64) {
    fail("Merkmale passen nicht zur Firmware");
    return false;
  }
  s_hann = (float *)malloc(s_win * sizeof(float));
  s_cos = (float *)malloc(s_nfft / 2 * sizeof(float));
  s_sin = (float *)malloc(s_nfft / 2 * sizeof(float));
  s_re = (float *)malloc(s_nfft * sizeof(float));
  s_im = (float *)malloc(s_nfft * sizeof(float));
  s_pw = (float *)malloc((s_nfft / 2 + 1) * sizeof(float));
  if (!s_hann || !s_cos || !s_sin || !s_re || !s_im || !s_pw) {
    fail("zu wenig Speicher");
    return false;
  }
  for (int i = 0; i < s_win; i++) s_hann[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / s_win);
  for (int i = 0; i < s_nfft / 2; i++) {
    s_cos[i] = cosf(2.0f * (float)M_PI * i / s_nfft);
    s_sin[i] = -sinf(2.0f * (float)M_PI * i / s_nfft);
  }
  int bits = 0;
  while ((1 << bits) < s_nfft) bits++;
  for (int i = 0; i < s_nfft; i++) {
    int r = 0;
    for (int b = 0; b < bits; b++) r |= ((i >> b) & 1) << (bits - 1 - b);
    s_rev[i] = (uint16_t)r;
  }
  // Dreiecksfilter (HTK-Mel, ohne Flächennormierung) auf den FFT-Bins
  int nbins = s_nfft / 2 + 1;
  float mlo = hz_to_mel(s_fmin), mhi = hz_to_mel(s_fmax);
  float pts[66];
  for (int i = 0; i < s_mels + 2; i++) pts[i] = mel_to_hz(mlo + (mhi - mlo) * i / (s_mels + 1));
  // Zwischenspeicher für alle Filter (nur beim Laden, deshalb vom Heap statt statisch)
  const int stride = NFFT_MAX / 2 + 1;
  float *tmpw = (float *)kws_alloc((size_t)s_mels * stride * sizeof(float));
  if (!tmpw) {
    fail("zu wenig Speicher");
    return false;
  }
  int total = 0;
  for (int m = 0; m < s_mels; m++) {
    float l = pts[m], c = pts[m + 1], r = pts[m + 2];
    int first = -1, last = -1;
    for (int k = 0; k < nbins; k++) {
      float f = (float)k * s_rate / s_nfft;
      float w = 0;
      if (f > l && f < c) w = (f - l) / (c - l);
      else if (f >= c && f < r) w = (r - f) / (r - c);
      tmpw[m * stride + k] = w;
      if (w > 0) {
        if (first < 0) first = k;
        last = k;
      }
    }
    if (first < 0) first = last = 0;
    s_mel_start[m] = first;
    s_mel_len[m] = last - first + 1;
    total += s_mel_len[m];
  }
  s_mel_w = (float *)kws_alloc(total * sizeof(float));
  s_buf = (float *)kws_alloc(s_win * sizeof(float));
  s_cmn = (float *)kws_alloc(s_mels * sizeof(float));
  s_ring = (float *)kws_alloc((size_t)s_frames * s_mels * sizeof(float));
  s_input = (float *)kws_alloc((size_t)s_frames * s_mels * sizeof(float));
  if (!s_mel_w || !s_buf || !s_cmn || !s_ring || !s_input) {
    free(tmpw);
    fail("zu wenig Speicher");
    return false;
  }
  int o = 0;
  for (int m = 0; m < s_mels; m++)
    for (int k = 0; k < s_mel_len[m]; k++) s_mel_w[o++] = tmpw[m * stride + s_mel_start[m] + k];
  free(tmpw);
  return true;
}

bool kws_load_mem(const uint8_t *buf, int len) {
  kws_unload();
  rd_t r = { buf, buf + len, false };
  if (len < 8 || memcmp(buf, "KWS1", 4) != 0) {
    fail("keine Modelldatei (KWS1)");
    return false;
  }
  r.p += 4;
  int ver = rd_u16(&r);
  s_rate = rd_u16(&r);
  s_win = rd_u16(&r);
  s_hop = rd_u16(&r);
  s_nfft = rd_u16(&r);
  s_mels = rd_u16(&r);
  s_frames = rd_u16(&r);
  s_infer_hops = rd_u16(&r);
  s_cmn_frames = rd_u16(&r);
  s_fmin = rd_f32(&r);
  s_fmax = rd_f32(&r);
  s_thr = rd_f32(&r);
  s_wake_thr = rd_f32(&r);
  if (ver != 1 || s_rate != KWS_RATE || r.bad || s_frames < 8 || s_mels < 4) {
    fail("Modellversion passt nicht");
    return false;
  }
  if (s_infer_hops < 1) s_infer_hops = 1;
  if (s_cmn_frames < 1) s_cmn_frames = 1;
  s_mean = (float *)kws_alloc(s_mels * sizeof(float));
  s_istd = (float *)kws_alloc(s_mels * sizeof(float));
  if (!s_mean || !s_istd) {
    fail("zu wenig Speicher");
    return false;
  }
  rd_floats(&r, s_mean, s_mels);
  rd_floats(&r, s_istd, s_mels);
  s_ncls = rd_u8(&r);
  if (s_ncls < 2 || s_ncls > KWS_MAX_CLASSES) {
    fail("Klassenzahl ungültig");
    return false;
  }
  for (int i = 0; i < s_ncls; i++) {
    int n = rd_u8(&r);
    if (!rd_has(&r, n)) break;
    int c = n < 23 ? n : 23;
    memcpy(s_cls[i], r.p, c);
    s_cls[i][c] = 0;
    r.p += n;
  }
  // Schichten: erst Formen ausrechnen und Gewichte zählen
  s_nlay = rd_u8(&r);
  if (s_nlay < 1 || s_nlay > MAX_LAYERS || r.bad) {
    fail("Schichten ungültig");
    return false;
  }
  const uint8_t *layers_start = r.p;
  int h = s_frames, w = s_mels, c = 1, flat = -1;  // flat >= 0: nach GAP/DENSE ein Vektor
  size_t nweights = 0;
  int maxact = h * w * c;
  for (int i = 0; i < s_nlay && !r.bad; i++) {
    layer_t *L = &s_lay[i];
    memset(L, 0, sizeof(*L));
    L->type = rd_u8(&r);
    L->ih = h;
    L->iw = w;
    L->ic = flat >= 0 ? flat : c;
    if (L->type == L_CONV || L->type == L_DW) {
      if (flat >= 0) {
        fail("Faltung nach Vektor");
        return false;
      }
      if (L->type == L_CONV) L->oc = rd_u16(&r);
      L->kh = rd_u8(&r);
      L->kw = rd_u8(&r);
      L->sh = rd_u8(&r);
      L->sw = rd_u8(&r);
      L->pad = rd_u8(&r);
      L->act = rd_u8(&r);
      if (L->type == L_DW) L->oc = c;
      if (!L->sh || !L->sw || !L->kh || !L->kw) {
        fail("Schicht ungültig");
        return false;
      }
      out_dim(h, L->kh, L->sh, L->pad, &L->oh, &L->pt);
      out_dim(w, L->kw, L->sw, L->pad, &L->ow, &L->pl);
      int nw = L->type == L_CONV ? L->oc * L->kh * L->kw * c : L->kh * L->kw * c;
      if (!rd_has(&r, (nw + L->oc) * 4)) break;
      r.p += (nw + L->oc) * 4;
      nweights += nw + L->oc;
      h = L->oh;
      w = L->ow;
      c = L->oc;
      if (h < 1 || w < 1) {
        fail("Schicht zu klein");
        return false;
      }
      if (h * w * c > maxact) maxact = h * w * c;
    } else if (L->type == L_GAP) {
      flat = c;
      L->oc = c;
    } else if (L->type == L_DENSE) {
      if (flat < 0) flat = h * w * c;
      L->ic = flat;
      L->oc = rd_u16(&r);
      L->act = rd_u8(&r);
      int nw = L->oc * flat;
      if (!rd_has(&r, (nw + L->oc) * 4)) break;
      r.p += (nw + L->oc) * 4;
      nweights += nw + L->oc;
      flat = L->oc;
      if (flat > maxact) maxact = flat;
    } else {
      fail("unbekannter Schichttyp");
      return false;
    }
  }
  if (r.bad || flat != s_ncls) {
    fail(r.bad ? "Modelldatei zu kurz" : "Ausgänge passen nicht zu den Klassen");
    return false;
  }
  int info_len = rd_u16(&r);
  if (!r.bad && info_len > 0 && rd_has(&r, info_len)) {
    int n = info_len < (int)sizeof(s_info) - 1 ? info_len : (int)sizeof(s_info) - 1;
    memcpy(s_info, r.p, n);
    s_info[n] = 0;
  } else {
    s_info[0] = 0;
  }
  // zweiter Durchgang: Gewichte ausgerichtet kopieren
  s_pool = (float *)kws_alloc(nweights * sizeof(float));
  s_act_len = maxact;
  s_act[0] = (float *)kws_alloc(maxact * sizeof(float));
  s_act[1] = (float *)kws_alloc(maxact * sizeof(float));
  if (!s_pool || !s_act[0] || !s_act[1]) {
    fail("zu wenig Speicher");
    return false;
  }
  r.p = layers_start;
  r.bad = false;
  float *pw = s_pool;
  for (int i = 0; i < s_nlay; i++) {
    layer_t *L = &s_lay[i];
    r.p += 1;  // Typ
    if (L->type == L_CONV || L->type == L_DW) {
      r.p += (L->type == L_CONV ? 2 : 0) + 6;
      int nw = L->type == L_CONV ? L->oc * L->kh * L->kw * L->ic : L->kh * L->kw * L->ic;
      L->w = pw;
      rd_floats(&r, pw, nw);
      pw += nw;
      L->b = pw;
      rd_floats(&r, pw, L->oc);
      pw += L->oc;
    } else if (L->type == L_DENSE) {
      r.p += 3;
      L->w = pw;
      rd_floats(&r, pw, L->oc * L->ic);
      pw += L->oc * L->ic;
      L->b = pw;
      rd_floats(&r, pw, L->oc);
      pw += L->oc;
    }
  }
  if (!setup_frontend()) return false;
  s_ready = true;
  s_err[0] = 0;
  kws_reset();
  return true;
}

bool kws_load_file(const char *path) {
  long n = storage_size(path);
  if (n <= 0) {
    kws_unload();
    fail("kein Modell auf der SD-Karte");
    return false;
  }
  if (n > 2 * 1024 * 1024) {
    fail("Modelldatei zu groß");
    return false;
  }
  uint8_t *buf = (uint8_t *)kws_alloc(n);
  if (!buf) {
    fail("zu wenig Speicher");
    return false;
  }
  int got = storage_read_bin(path, buf, (int)n);
  bool ok = got == n && kws_load_mem(buf, (int)n);
  if (got != n) fail("Modell nicht lesbar");
  free(buf);
  return ok;
}

// ------------------------------------------------------------
//  Merkmale
// ------------------------------------------------------------
void kws_reset() {
  if (!s_ready) return;
  memset(s_buf, 0, s_win * sizeof(float));
  s_pend = 0;
  s_hp_x1 = s_hp_y1 = 0;
  s_cmn_init = false;
  s_ring_pos = s_ring_fill = s_since_infer = 0;
  s_hist_n = s_hist_pos = 0;
  memset(s_probs, 0, sizeof(s_probs));
}

// Ein Rahmen aus s_buf: Fenster, FFT, Leistung, Mel, Log, CMN, Normierung
static void frame_features(float *out) {
  float *re = s_re, *im = s_im;
  for (int i = 0; i < s_nfft; i++) {
    int j = s_rev[i];
    re[i] = j < s_win ? s_buf[j] * s_hann[j] : 0.0f;
    im[i] = 0.0f;
  }
  for (int len = 2; len <= s_nfft; len <<= 1) {
    int half = len >> 1, step = s_nfft / len;
    for (int i = 0; i < s_nfft; i += len)
      for (int k = 0; k < half; k++) {
        float wr = s_cos[k * step], wi = s_sin[k * step];
        int a = i + k, b = a + half;
        float tr = re[b] * wr - im[b] * wi;
        float ti = re[b] * wi + im[b] * wr;
        re[b] = re[a] - tr;
        im[b] = im[a] - ti;
        re[a] += tr;
        im[a] += ti;
      }
  }
  float *pw = s_pw;
  for (int k = 0; k <= s_nfft / 2; k++) pw[k] = re[k] * re[k] + im[k] * im[k];
  const float *mw = s_mel_w;
  float a = 1.0f / s_cmn_frames;
  for (int m = 0; m < s_mels; m++) {
    float e = 0;
    const float *p = pw + s_mel_start[m];
    for (int k = 0; k < s_mel_len[m]; k++) e += mw[k] * p[k];
    mw += s_mel_len[m];
    float x = logf(e + 1e-7f);
    if (!s_cmn_init) s_cmn[m] = x;
    float y = x - s_cmn[m];
    s_cmn[m] += a * (x - s_cmn[m]);
    out[m] = (y - s_mean[m]) * s_istd[m];
  }
  s_cmn_init = true;
}

// Neue Werte: Hochpass gegen den Gleichanteil, je hop Werte ein Rahmen
static int push_samples(const int16_t *pcm, int n, float *frames_out, int max_frames) {
  int made = 0;
  for (int i = 0; i < n; i++) {
    float x = pcm[i] * (1.0f / 32768.0f);
    float y = x - s_hp_x1 + 0.995f * s_hp_y1;
    s_hp_x1 = x;
    s_hp_y1 = y;
    s_buf[s_win - s_hop + s_pend] = y;
    if (++s_pend == s_hop) {
      s_pend = 0;
      float *dst = frames_out ? (made < max_frames ? frames_out + made * s_mels : NULL)
                              : s_ring + s_ring_pos * s_mels;
      if (dst) frame_features(dst);
      memmove(s_buf, s_buf + s_hop, (s_win - s_hop) * sizeof(float));
      if (!frames_out) {
        s_ring_pos = (s_ring_pos + 1) % s_frames;
        if (s_ring_fill < s_frames) s_ring_fill++;
        s_since_infer++;
      }
      made++;
    }
  }
  return made;
}

int kws_features(const int16_t *pcm, int n, float *out, int max_frames) {
  if (!s_ready) return 0;
  kws_reset();
  int made = push_samples(pcm, n, out, max_frames);
  kws_reset();
  return made < max_frames ? made : max_frames;
}

// ------------------------------------------------------------
//  Netz
// ------------------------------------------------------------
static void run_conv(const layer_t *L, const float *in, float *out) {
  const int ic = L->ic, oc = L->oc;
  if (L->kh == 1 && L->kw == 1 && L->sh == 1 && L->sw == 1) {  // punktweise: schneller Weg
    int n = L->oh * L->ow;
    for (int p = 0; p < n; p++) {
      const float *x = in + p * ic;
      float *y = out + p * oc;
      const float *wp = L->w;
      for (int o = 0; o < oc; o++) {
        float s = L->b[o];
        for (int i = 0; i < ic; i++) s += wp[i] * x[i];
        wp += ic;
        y[o] = (L->act && s < 0) ? 0 : s;
      }
    }
    return;
  }
  if (ic == 1 && L->kh * L->kw <= 64) {  // erste Schicht: Ausschnitt einmal holen, dann je Kanal ein Skalarprodukt
    const int kk = L->kh * L->kw;
    float patch[64];
    for (int oy = 0; oy < L->oh; oy++)
      for (int ox = 0; ox < L->ow; ox++) {
        int n = 0;
        for (int ky = 0; ky < L->kh; ky++) {
          int iy = oy * L->sh - L->pt + ky;
          for (int kx = 0; kx < L->kw; kx++) {
            int ix = ox * L->sw - L->pl + kx;
            patch[n++] = (iy < 0 || iy >= L->ih || ix < 0 || ix >= L->iw) ? 0.0f : in[iy * L->iw + ix];
          }
        }
        float *y = out + (oy * L->ow + ox) * oc;
        const float *wp = L->w;
        for (int o = 0; o < oc; o++) {
          float s = L->b[o];
          for (int k = 0; k < kk; k++) s += wp[k] * patch[k];
          wp += kk;
          y[o] = (L->act && s < 0) ? 0 : s;
        }
      }
    return;
  }
  for (int oy = 0; oy < L->oh; oy++)
    for (int ox = 0; ox < L->ow; ox++) {
      float *y = out + (oy * L->ow + ox) * oc;
      for (int o = 0; o < oc; o++) y[o] = L->b[o];
      for (int ky = 0; ky < L->kh; ky++) {
        int iy = oy * L->sh - L->pt + ky;
        if (iy < 0 || iy >= L->ih) continue;
        for (int kx = 0; kx < L->kw; kx++) {
          int ix = ox * L->sw - L->pl + kx;
          if (ix < 0 || ix >= L->iw) continue;
          const float *x = in + (iy * L->iw + ix) * ic;
          const float *wp = L->w + (ky * L->kw + kx) * ic;
          int stride = L->kh * L->kw * ic;
          for (int o = 0; o < oc; o++) {
            float s = 0;
            const float *wo = wp + o * stride;
            for (int i = 0; i < ic; i++) s += wo[i] * x[i];
            y[o] += s;
          }
        }
      }
      if (L->act)
        for (int o = 0; o < oc; o++)
          if (y[o] < 0) y[o] = 0;
    }
}

static void run_dw(const layer_t *L, const float *in, float *out) {
  const int c = L->ic;
  for (int oy = 0; oy < L->oh; oy++)
    for (int ox = 0; ox < L->ow; ox++) {
      float *y = out + (oy * L->ow + ox) * c;
      for (int k = 0; k < c; k++) y[k] = L->b[k];
      for (int ky = 0; ky < L->kh; ky++) {
        int iy = oy * L->sh - L->pt + ky;
        if (iy < 0 || iy >= L->ih) continue;
        for (int kx = 0; kx < L->kw; kx++) {
          int ix = ox * L->sw - L->pl + kx;
          if (ix < 0 || ix >= L->iw) continue;
          const float *x = in + (iy * L->iw + ix) * c;
          const float *wp = L->w + (ky * L->kw + kx) * c;
          for (int k = 0; k < c; k++) y[k] += wp[k] * x[k];
        }
      }
      if (L->act)
        for (int k = 0; k < c; k++)
          if (y[k] < 0) y[k] = 0;
    }
}

bool kws_run(const float *feat, float *probs) {
  if (!s_ready) return false;
  const float *in = feat;
  int cur = 0, len = s_frames * s_mels;
  for (int i = 0; i < s_nlay; i++) {
    const layer_t *L = &s_lay[i];
    float *out = s_act[cur];
    if (L->type == L_CONV) {
      run_conv(L, in, out);
      len = L->oh * L->ow * L->oc;
    } else if (L->type == L_DW) {
      run_dw(L, in, out);
      len = L->oh * L->ow * L->oc;
    } else if (L->type == L_GAP) {
      int c = L->ic, n = len / c;
      for (int k = 0; k < c; k++) out[k] = 0;
      for (int p = 0; p < n; p++)
        for (int k = 0; k < c; k++) out[k] += in[p * c + k];
      for (int k = 0; k < c; k++) out[k] /= n;
      len = c;
    } else {  // DENSE
      for (int o = 0; o < L->oc; o++) {
        float s = L->b[o];
        const float *wp = L->w + o * L->ic;
        for (int k = 0; k < L->ic; k++) s += wp[k] * in[k];
        out[o] = (L->act && s < 0) ? 0 : s;
      }
      len = L->oc;
    }
    in = out;
    cur ^= 1;
  }
  // Softmax
  float mx = in[0];
  for (int k = 1; k < s_ncls; k++)
    if (in[k] > mx) mx = in[k];
  float sum = 0;
  for (int k = 0; k < s_ncls; k++) {
    probs[k] = expf(in[k] - mx);
    sum += probs[k];
  }
  for (int k = 0; k < s_ncls; k++) probs[k] /= sum;
  return true;
}

bool kws_feed(const int16_t *pcm, int n) {
  if (!s_ready) return false;
  push_samples(pcm, n, NULL, 0);
  if (s_ring_fill < s_frames || s_since_infer < s_infer_hops) return false;
  s_since_infer = 0;
  // Ring in zeitlicher Reihenfolge (älteste zuerst) ins Eingangsfeld
  int first = s_ring_pos;  // nächste Schreibstelle = ältester Rahmen
  for (int t = 0; t < s_frames; t++)
    memcpy(s_input + t * s_mels, s_ring + ((first + t) % s_frames) * s_mels, s_mels * sizeof(float));
  uint32_t t0 = kws_us();
  float p[KWS_MAX_CLASSES];
  kws_run(s_input, p);
  s_infer_us = kws_us() - t0;
  memcpy(s_hist[s_hist_pos], p, sizeof(float) * s_ncls);
  s_hist_pos = (s_hist_pos + 1) % SMOOTH;
  if (s_hist_n < SMOOTH) s_hist_n++;
  for (int k = 0; k < s_ncls; k++) {
    float s = 0;
    for (int j = 0; j < s_hist_n; j++) s += s_hist[j][k];
    s_probs[k] = s / s_hist_n;
  }
  return true;
}

int kws_best(float *p) {
  int best = -1;
  float bp = 0;
  for (int k = 0; k < s_ncls; k++) {
    if (s_cls[k][0] == '_') continue;
    if (s_probs[k] > bp) {
      bp = s_probs[k];
      best = k;
    }
  }
  if (p) *p = bp;
  return best;
}
