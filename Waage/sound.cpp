#include "sound.h"
#include "settings.h"
#include "hal.h"
#include "data.h"
#include "storage.h"
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

// ------------------------------------------------------------
//  Parkpiepser-Logik (plattformunabhängig)
// ------------------------------------------------------------
static int s_park_state = -1;
static uint32_t s_park_last = 0;

void sound_parking_reset() {
  s_park_state = -1;
  s_park_last = 0;
}

void sound_parking(float grams, float goal, float tol) {
  if (goal <= 0) return;
  uint32_t now = hal_millis();
  float rest = goal - grams;
  int state = rest > tol ? 0 : (rest >= -tol ? 1 : 2);

  if (state == 1) {  // im Ziel: einmal langer Ton
    if (s_park_state != 1) sound_play(SND_REACHED);
  } else if (state == 2) {  // zu viel: tiefer Ton alle 0,7 s
    if (s_park_state != 2 || now - s_park_last > 700) {
      sound_play(SND_OVERLOAD);
      s_park_last = now;
    }
  } else {
    // erst ab der Hälfte piepen; Abstand 1 s ... 0,1 s
    float frac = rest / goal;
    if (frac < 0.5f && grams > 0.5f) {
      uint32_t interval = 100 + (uint32_t)(frac * 2.0f * 900.0f);
      if (now - s_park_last >= interval) {
        sound_play(SND_PARK);
        s_park_last = now;
      }
    }
  }
  s_park_state = state;
}

const char *scheme_name(int scheme) {
  static const char *const N[SCHEME_COUNT] = { "Klassisch", "Sanft", "Retro", "Minimal" };
  return N[(scheme < 0 || scheme >= SCHEME_COUNT) ? 0 : scheme];
}

#ifdef ARDUINO
// ============================================================
//  Tonerzeugung und WAV-Wiedergabe auf dem ESP32
// ============================================================
#include <Arduino.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <SD_MMC.h>
#include "storage.h"

#define PIN_I2S_BCLK 48
#define PIN_I2S_LRC  38
#define PIN_I2S_DOUT 47
#define RATE 16000
#define VOICE_DIR "/Waage/Stimme"
#define SPEAK_MAX 24        // so viele Bausteine je Ansage
#define DEBOUNCE_MS 60      // gleicher Ton wird so lange unterdrückt

typedef struct {
  uint16_t freq;  // 0 = Pause
  uint16_t ms;
} tone_step_t;

// Kategorien: kurze Töne dürfen unterbrochen werden, lange laufen zu Ende
typedef enum { CAT_CLICK, CAT_SIGNAL, CAT_LONG } cat_t;

static const tone_step_t N_ROTARY[]   = { { 2800, 7 }, { 0, 0 } };
static const tone_step_t N_CLICK[]    = { { 2400, 12 }, { 0, 0 } };
static const tone_step_t N_TARA[]     = { { 1760, 70 }, { 0, 0 } };
static const tone_step_t N_SAVE[]     = { { 1760, 60 }, { 0, 40 }, { 2350, 90 }, { 0, 0 } };
static const tone_step_t N_WARN[]     = { { 660, 120 }, { 0, 0 } };
static const tone_step_t N_OVERLOAD[] = { { 330, 220 }, { 0, 0 } };
static const tone_step_t N_TICK[]     = { { 1320, 35 }, { 0, 0 } };
static const tone_step_t N_POT[]      = { { 1320, 60 }, { 0, 30 }, { 1760, 60 }, { 0, 30 }, { 2350, 90 }, { 0, 0 } };
static const tone_step_t N_DONE[]     = { { 1320, 90 }, { 1760, 90 }, { 2090, 90 }, { 2640, 200 }, { 0, 0 } };
static const tone_step_t N_PARK[]     = { { 1760, 40 }, { 0, 0 } };
static const tone_step_t N_REACHED[]  = { { 2090, 600 }, { 0, 0 } };
static const tone_step_t N_TEST[]     = { { 1760, 150 }, { 0, 0 } };
static const tone_step_t N_DRUM[]     = { { 180, 25 }, { 0, 35 }, { 180, 25 }, { 0, 35 }, { 200, 25 }, { 0, 30 },
                                          { 200, 25 }, { 0, 30 }, { 220, 25 }, { 0, 25 }, { 240, 25 }, { 0, 25 },
                                          { 260, 25 }, { 0, 20 }, { 280, 25 }, { 0, 20 }, { 1320, 90 }, { 1760, 90 },
                                          { 2640, 250 }, { 0, 0 } };

typedef struct {
  const tone_step_t *pat;
  cat_t cat;
  bool minimal;  // auch im Schema "Minimal" hörbar?
} snd_def_t;

// Reihenfolge wie in sound_t!
static const snd_def_t DEFS[] = {
  { N_ROTARY, CAT_CLICK, false },  { N_CLICK, CAT_CLICK, false },  { N_TARA, CAT_SIGNAL, false },
  { N_SAVE, CAT_SIGNAL, false },   { N_WARN, CAT_SIGNAL, true },   { N_OVERLOAD, CAT_SIGNAL, true },
  { N_TICK, CAT_CLICK, false },    { N_POT, CAT_SIGNAL, false },   { N_DONE, CAT_LONG, false },
  { N_PARK, CAT_CLICK, true },     { N_REACHED, CAT_LONG, true },  { N_TEST, CAT_SIGNAL, true },
  { N_DRUM, CAT_LONG, false },
};

// Nachricht an den Ton-Task
typedef struct {
  uint8_t type;  // 0 = Ton, 1 = Ansage
  uint8_t sound;
  uint8_t count;
  char words[SPEAK_MAX][14];
} msg_t;

static I2SClass s_i2s;
static QueueHandle_t s_queue = NULL;
static volatile bool s_abort = false;
static volatile bool s_playing_long = false;
static volatile bool s_speaking = false;
static bool s_voice = false;
static uint32_t s_voice_check = 0;
static uint32_t s_last_ms = 0;
static uint8_t s_last_snd = 255;

// ------------------------------------------------------------
//  Tonausgabe
// ------------------------------------------------------------
static float scheme_amp(cat_t cat) {
  float amp = 9000.0f * g_set.volume / 100.0f;
  if (cat == CAT_CLICK) amp *= 0.45f;  // Klicks deutlich leiser
  if (g_set.scheme == 1) amp *= 0.7f;  // Sanft
  return amp;
}

static void play_note(uint16_t freq, uint16_t ms, cat_t cat) {
  static int16_t buf[256 * 2];
  if (g_set.scheme == 1) {  // Sanft: tiefer und weicher
    freq = (uint16_t)(freq * 0.75f);
    ms = (uint16_t)(ms * 1.2f);
  }
  int total = RATE * ms / 1000;
  int fade = RATE * (g_set.scheme == 1 ? 8 : 3) / 1000;
  float amp = scheme_amp(cat);
  float phase = 0, step = 2.0f * (float)M_PI * freq / RATE;
  int done = 0;
  while (done < total) {
    if (s_abort) return;
    int n = total - done > 256 ? 256 : total - done;
    for (int i = 0; i < n; i++) {
      int k = done + i;
      float env = 1.0f;
      if (k < fade) env = (float)k / fade;
      else if (k > total - fade) env = (float)(total - k) / fade;
      float w = sinf(phase);
      if (g_set.scheme == 2) w = w >= 0 ? 0.7f : -0.7f;  // Retro: Rechteck
      int16_t v = freq ? (int16_t)(w * amp * env) : 0;
      phase += step;
      if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
      buf[2 * i] = v;
      buf[2 * i + 1] = v;
    }
    s_i2s.write((uint8_t *)buf, n * 4);
    done += n;
  }
}

// ------------------------------------------------------------
//  WAV-Wiedergabe (16 Bit PCM; andere Abtastraten werden einfach
//  umgerechnet)
// ------------------------------------------------------------
static bool play_wav(const char *name) {
  char path[96];
  if (!storage_ok()) return false;
  if (g_set.voice[0]) snprintf(path, sizeof(path), VOICE_DIR "/%s/%s.wav", g_set.voice, name);
  else snprintf(path, sizeof(path), VOICE_DIR "/%s.wav", name);
  storage_lock();
  File f = SD_MMC.open(path, FILE_READ);
  uint8_t hdr[44];
  bool good = f && f.read(hdr, 44) == 44 && !memcmp(hdr, "RIFF", 4) && !memcmp(hdr + 8, "WAVE", 4);
  storage_unlock();
  if (!good) {
    if (f) {
      storage_lock();
      f.close();
      storage_unlock();
    }
    return false;
  }
  uint16_t channels = hdr[22] | (hdr[23] << 8);
  uint32_t rate = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | ((uint32_t)hdr[27] << 24);
  uint16_t bits = hdr[34] | (hdr[35] << 8);
  if (bits != 16 || channels < 1 || channels > 2 || rate < 8000) {
    storage_lock();
    f.close();
    storage_unlock();
    return false;
  }

  static int16_t in[512];
  static int16_t out[1024 * 2];
  float gain = (float)g_set.volume / 100.0f;
  float step = (float)rate / RATE;  // >1: Datei hat mehr Abtastwerte als nötig
  float pos = 0;
  while (!s_abort) {
    storage_lock();  // nur für das Lesen sperren, nicht während der Ausgabe
    int n = storage_ok() ? f.read((uint8_t *)in, sizeof(in)) : -1;
    storage_unlock();
    if (n <= 0) break;
    int frames = n / 2 / channels;
    int o = 0;
    while (pos < frames && o < 1024) {
      int16_t v = in[(int)pos * channels];
      int16_t s = (int16_t)(v * gain);
      out[2 * o] = s;
      out[2 * o + 1] = s;
      o++;
      pos += step;
    }
    pos -= frames;
    if (pos < 0) pos = 0;
    if (o > 0) s_i2s.write((uint8_t *)out, o * 4);
  }
  storage_lock();
  f.close();
  storage_unlock();
  return true;
}

// ------------------------------------------------------------
//  Zahlen auf Deutsch in Bausteine zerlegen
// ------------------------------------------------------------
static const char *const ONES[20] = { "null", "eins", "zwei", "drei", "vier", "fuenf", "sechs",
                                      "sieben", "acht", "neun", "zehn", "elf", "zwoelf", "dreizehn",
                                      "vierzehn", "fuenfzehn", "sechzehn", "siebzehn", "achtzehn", "neunzehn" };
static const char *const TENS[10] = { "", "", "zwanzig", "dreissig", "vierzig", "fuenfzig",
                                      "sechzig", "siebzig", "achtzig", "neunzig" };

static void add_word(msg_t *m, const char *w) {
  if (m->count >= SPEAK_MAX) return;
  strncpy(m->words[m->count], w, 13);
  m->words[m->count][13] = 0;
  m->count++;
}

// ------------------------------------------------------------
//  Zahlen auf Englisch: 1234 -> one thousand two hundred thirty four
//  (Dateinamen = englische Wörter, siehe stimme_erzeugen.py --lang en)
// ------------------------------------------------------------
static const char *const ONES_EN[20] = { "zero", "one", "two", "three", "four", "five", "six",
                                         "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen",
                                         "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen" };
static const char *const TENS_EN[10] = { "", "", "twenty", "thirty", "forty", "fifty",
                                         "sixty", "seventy", "eighty", "ninety" };

static void add_number_en(msg_t *m, int n) {
  if (n >= 1000) {
    add_number_en(m, n / 1000);
    add_word(m, "thousand");
    n %= 1000;
    if (n == 0) return;
  }
  if (n >= 100) {
    add_word(m, ONES_EN[n / 100]);
    add_word(m, "hundred");
    n %= 100;
    if (n == 0) return;
  }
  if (n < 20) {
    if (n > 0 || m->count == 0) add_word(m, ONES_EN[n]);
    return;
  }
  add_word(m, TENS_EN[n / 10]);
  if (n % 10) add_word(m, ONES_EN[n % 10]);
}

static void add_number(msg_t *m, int n) {
  if (g_set.lang == LANG_EN) {
    add_number_en(m, n);
    return;
  }
  if (n >= 1000) {
    int t = n / 1000;
    if (t == 1) add_word(m, "ein");
    else add_number(m, t);
    add_word(m, "tausend");
    n %= 1000;
    if (n == 0) return;
  }
  if (n >= 100) {
    int h = n / 100;
    add_word(m, h == 1 ? "ein" : ONES[h]);
    add_word(m, "hundert");
    n %= 100;
    if (n == 0) return;
  }
  if (n < 20) {
    if (n > 0 || m->count == 0) add_word(m, ONES[n]);
    return;
  }
  int o = n % 10, t = n / 10;
  if (o > 0) {
    add_word(m, o == 1 ? "ein" : ONES[o]);  // "einundzwanzig"
    add_word(m, "und");
  }
  add_word(m, TENS[t]);
}

// ------------------------------------------------------------
//  Task
// ------------------------------------------------------------
static bool voice_check();

static void sound_task(void *arg) {
  static msg_t m;
  while (true) {
    if (xQueueReceive(s_queue, &m, portMAX_DELAY) != pdTRUE) continue;
    s_abort = false;
    if (m.type == 0) {
      const snd_def_t *d = &DEFS[m.sound];
      s_playing_long = d->cat == CAT_LONG;
      for (const tone_step_t *n = d->pat; n->ms; n++) play_note(n->freq, n->ms, d->cat);
      play_note(0, 15, d->cat);
      s_playing_long = false;
    } else {
      s_speaking = true;
      for (int i = 0; i < m.count && !s_abort; i++) play_wav(m.words[i]);
      play_note(0, 15, CAT_SIGNAL);
      s_speaking = false;
    }
  }
}

void sound_begin() {
  s_i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
  if (!s_i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    printf("Sound: I2S-Start fehlgeschlagen\r\n");
    return;
  }
  s_voice = voice_check();
  printf("Sound: Stimme %s\r\n", s_voice ? "gefunden" : "nicht gefunden");
  s_queue = xQueueCreate(1, sizeof(msg_t));
  xTaskCreatePinnedToCore(sound_task, "Sound", 6144, NULL, 2, NULL, 0);  // liest auch WAVs von der SD
}

// Prüft, ob das eingestellte Paket (oder die Dateien direkt im Ordner) da sind
// Woran ein Stimmpaket seine Sprache erkennen lässt: Deutsch hat gramm.wav, Englisch grams.wav
static const char *check_word() {
  return g_set.lang == LANG_EN ? "grams" : "gramm";
}

// Hat der Ordner (Paketname, "" = direkt in /Waage/Stimme) Bausteine der eingestellten Sprache?
static bool pack_ok(const char *pack) {
  char path[96];
  if (pack[0]) snprintf(path, sizeof(path), VOICE_DIR "/%s/%s.wav", pack, check_word());
  else snprintf(path, sizeof(path), VOICE_DIR "/%s.wav", check_word());
  return storage_exists(path);
}

static bool voice_check() {
  if (!storage_ok()) return false;
  return pack_ok(g_set.voice);
}

bool sound_voice_available() {
  if (!storage_ok()) {  // Karte weg -> später neu prüfen
    s_voice = false;
    return false;
  }
  // Karte nachträglich eingesteckt oder Paket gewechselt? Alle 2 s nachsehen.
  if (!s_voice && millis() - s_voice_check > 2000) {
    s_voice_check = millis();
    if (!voice_check() && !g_set.voice[0]) {
      // noch kein Paket gewählt: erstes gefundenes nehmen
      static char packs[VOICE_PACKS_MAX][24];
      int n = sound_voice_packs(packs, VOICE_PACKS_MAX);
      if (n > 0) {
        strncpy(g_set.voice, packs[0], sizeof(g_set.voice) - 1);
        g_set.voice[sizeof(g_set.voice) - 1] = 0;
      }
    }
    s_voice = voice_check();
  }
  return s_voice;
}

int sound_voice_packs(char names[][24], int max) {
  // Alle Unterordner lesen (beide Sprachen), dann nur die passenden behalten
  static char dirs[64][STORAGE_NAME_LEN];
  int n = storage_list_dirs(VOICE_DIR, dirs, 64);
  int k = 0;
  for (int i = 0; i < n && k < max; i++) {
    if (!pack_ok(dirs[i])) continue;  // nur Pakete der eingestellten Sprache
    strncpy(names[k], dirs[i], 23);
    names[k][23] = 0;
    k++;
  }
  // alphabetisch sortieren (Groß-/Kleinschreibung egal)
  for (int i = 1; i < k; i++)
    for (int j = i; j > 0 && strcasecmp(names[j], names[j - 1]) < 0; j--) {
      char t[24];
      memcpy(t, names[j], 24);
      memcpy(names[j], names[j - 1], 24);
      memcpy(names[j - 1], t, 24);
    }
  return k;
}

// Sprache umgestellt: passt das Paket nicht, das erste passende nehmen
void sound_lang_changed() {
  if (storage_ok() && !pack_ok(g_set.voice)) {
    static char packs[VOICE_PACKS_MAX][24];
    if (sound_voice_packs(packs, VOICE_PACKS_MAX) > 0) {
      strncpy(g_set.voice, packs[0], sizeof(g_set.voice) - 1);
      g_set.voice[sizeof(g_set.voice) - 1] = 0;
    }
  }
  s_voice = voice_check();
  s_voice_check = millis();
}

void sound_voice_set(const char *pack) {
  strncpy(g_set.voice, pack, sizeof(g_set.voice) - 1);
  g_set.voice[sizeof(g_set.voice) - 1] = 0;
  s_voice = voice_check();
  s_voice_check = millis();
}

void sound_voice_next() {
  static char packs[VOICE_PACKS_MAX][24];
  int n = sound_voice_packs(packs, VOICE_PACKS_MAX);
  if (n == 0) return;
  int cur = -1;
  for (int i = 0; i < n; i++)
    if (strcmp(packs[i], g_set.voice) == 0) cur = i;
  strncpy(g_set.voice, packs[(cur + 1) % n], sizeof(g_set.voice) - 1);
  g_set.voice[sizeof(g_set.voice) - 1] = 0;
  s_voice = voice_check();
  s_voice_check = millis();
}

void sound_play(sound_t s) {
  if (!s_queue || g_set.volume <= 0) return;
  if (g_set.scheme == 3 && !DEFS[s].minimal) return;  // Schema "Minimal"

  uint32_t now = millis();
  if (s == s_last_snd && now - s_last_ms < DEBOUNCE_MS) return;  // Doppelauslösung
  s_last_snd = (uint8_t)s;
  s_last_ms = now;

  // Laufenden kurzen Ton abbrechen, lange Signale und Ansagen nicht
  if (!s_playing_long && !s_speaking) s_abort = true;

  static msg_t m;
  m.type = 0;
  m.sound = (uint8_t)s;
  m.count = 0;
  xQueueOverwrite(s_queue, &m);  // nur der neueste Ton wartet
}

static void speak(msg_t *m) {
  if (!s_queue || g_set.volume <= 0 || !g_set.speak || !sound_voice_available()) return;
  s_abort = true;  // laufende Ansage ersetzen
  xQueueOverwrite(s_queue, m);
}

void sound_speak_weight(float grams, int unit) {
  static msg_t m;
  m.type = 1;
  m.count = 0;
  float v = unit_from_g(grams, unit);
  bool en = g_set.lang == LANG_EN;
  if (v < 0) {
    add_word(&m, "minus");  // auf Englisch derselbe Dateiname
    v = -v;
  }
  int whole = (int)v;
  add_number(&m, whole);
  int dec = unit_decimals(unit);
  if (dec > 0) {
    int frac = (int)lroundf((v - whole) * powf(10, dec));
    if (frac > 0) {
      add_word(&m, en ? "point" : "komma");
      char digits[8];
      snprintf(digits, sizeof(digits), "%0*d", dec, frac);
      for (int i = 0; digits[i]; i++) add_word(&m, (en ? ONES_EN : ONES)[digits[i] - '0']);
    }
  }
  static const char *const UNIT_WORD[5] = { "gramm", "kilogramm", "unze", "pfund", "milliliter" };
  static const char *const UNIT_WORD_EN[5] = { "grams", "kilograms", "ounces", "pounds", "milliliters" };
  add_word(&m, (en ? UNIT_WORD_EN : UNIT_WORD)[(unit < 0 || unit > 4) ? 0 : unit]);
  speak(&m);
}

void sound_speak_count(int pieces) {
  static msg_t m;
  m.type = 1;
  m.count = 0;
  add_number(&m, pieces);
  add_word(&m, g_set.lang == LANG_EN ? "pieces" : "stueck");
  speak(&m);
}

// Feste Ansagen werden im Code mit dem deutschen Namen aufgerufen,
// auf Englisch wird die Datei mit dem englischen Namen gespielt.
// Dateinamen höchstens 13 Zeichen (siehe add_word)
static const char *const WORD_EN[][2] = {
  { "tara", "tare" },          { "gespeichert", "saved" },      { "ziel_erreicht", "goal_reached" },
  { "ueberlast", "overload" }, { "topf_erkannt", "pot_detected" }, { "fertig", "done" },
  { "waage_leer", "scale_empty" }, { "meter", "meters" },
};

void sound_speak_word(const char *file) {
  static msg_t m;
  m.type = 1;
  m.count = 0;
  if (g_set.lang == LANG_EN)
    for (unsigned i = 0; i < sizeof(WORD_EN) / sizeof(WORD_EN[0]); i++)
      if (strcmp(file, WORD_EN[i][0]) == 0) file = WORD_EN[i][1];
  add_word(&m, file);
  speak(&m);
}

#else
// Host-Test: stumm
void sound_begin() {}
void sound_play(sound_t s) { (void)s; }
// Stimmpakete wie auf dem Gerät auflisten (für die Auswahlseite im Simulator)
static bool pack_ok_host(const char *pack) {
  char path[96];
  snprintf(path, sizeof(path), "/Waage/Stimme/%s/%s.wav", pack, g_set.lang == LANG_EN ? "grams" : "gramm");
  return storage_exists(path);
}
bool sound_voice_available() { return g_set.voice[0] && pack_ok_host(g_set.voice); }
void sound_speak_weight(float grams, int unit) { (void)grams; (void)unit; }
void sound_speak_count(int pieces) { (void)pieces; }
void sound_speak_word(const char *file) { (void)file; }
int sound_voice_packs(char names[][24], int max) {
  static char dirs[64][STORAGE_NAME_LEN];
  int n = storage_list_dirs("/Waage/Stimme", dirs, 64), k = 0;
  for (int i = 0; i < n && k < max; i++)
    if (pack_ok_host(dirs[i])) { strncpy(names[k], dirs[i], 23); names[k][23] = 0; k++; }
  for (int i = 1; i < k; i++)
    for (int j = i; j > 0 && strcasecmp(names[j], names[j - 1]) < 0; j--) {
      char t[24]; memcpy(t, names[j], 24); memcpy(names[j], names[j - 1], 24); memcpy(names[j - 1], t, 24);
    }
  return k;
}
void sound_voice_next() {}
void sound_voice_set(const char *pack) { strncpy(g_set.voice, pack, sizeof(g_set.voice) - 1); g_set.voice[sizeof(g_set.voice) - 1] = 0; }
void sound_lang_changed() {}
#endif
