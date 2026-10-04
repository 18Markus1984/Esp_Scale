// ============================================================
//  Feinausrichtung des Displays (siehe disp_rot.h)
// ============================================================
#include "disp_rot.h"
#include <lvgl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
static void *big_alloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
#else
static void *big_alloc(size_t n) { return malloc(n); }
#endif

static int s_tenths = 0;
static bool s_installed = false;
static lv_disp_drv_t *s_drv = NULL;
static void (*s_orig_flush)(lv_disp_drv_t *, const lv_area_t *, lv_color_t *) = NULL;
static void (*s_orig_read)(lv_indev_drv_t *, lv_indev_data_t *) = NULL;
static lv_disp_draw_buf_t s_buf;
static lv_color_t *s_fb = NULL;   // LVGL zeichnet hier hinein (Vollbild)
static lv_area_t s_dirty;         // geänderter Bereich dieses Bildes
static bool s_have_dirty = false;
static int32_t s_cos = 65536, s_sin = 0;  // 16.16
// Gespeicherter Originalzustand des Treibers (für disp_rot_suspend)
static lv_disp_draw_buf_t *s_orig_buf = NULL;
static uint32_t s_orig_direct = 0, s_orig_full = 0;
static lv_indev_drv_t *s_indev_drv = NULL;
static void update_trig() {
  float a = s_tenths * 0.1f * (float)M_PI / 180.0f;
  s_cos = (int32_t)lroundf(cosf(a) * 65536.0f);
  s_sin = (int32_t)lroundf(sinf(a) * 65536.0f);
}

// Ausgabepunkt (x,y) -> Quellpunkt im Vollbild (16.16), Drehung um die Mitte
static inline void src_of(int32_t x, int32_t y, int32_t w, int32_t h, int32_t *sx, int32_t *sy) {
  int32_t dx = (x << 1) - (w - 1), dy = (y << 1) - (h - 1);  // doppelte Koordinate relativ zur Mitte
  *sx = ((dx * s_cos + dy * s_sin) >> 1) + ((w - 1) << 15);
  *sy = ((-dx * s_sin + dy * s_cos) >> 1) + ((h - 1) << 15);
}

// ------------------------------------------------------------
//  Bilinear drehen, schnell: RGB565 wird so "aufgespreizt", dass Rot, Grün
//  und Blau in einem 32-Bit-Wort Platz für eine 5-Bit-Gewichtung haben.
//  Ein Mischschritt kostet dann nur eine Multiplikation pro Farbpaar.
// ------------------------------------------------------------
static inline uint32_t spread(lv_color_t c) {
#if LV_COLOR_16_SWAP
  uint32_t v = __builtin_bswap16(c.full);
#else
  uint32_t v = c.full;
#endif
  return (v | (v << 16)) & 0x07E0F81Fu;
}

static inline lv_color_t pack(uint32_t s) {
  uint16_t v = (uint16_t)((s | (s >> 16)) & 0xFFFF);
  lv_color_t c;
#if LV_COLOR_16_SWAP
  c.full = __builtin_bswap16(v);
#else
  c.full = v;
#endif
  return c;
}

// a + (b - a) * w / 32, auf allen drei Farben gleichzeitig
static inline uint32_t lerp(uint32_t a, uint32_t b, uint32_t w) {
  return ((a * (32 - w) + b * w) >> 5) & 0x07E0F81Fu;
}

static inline uint32_t px_s(int32_t x, int32_t y, int32_t w, int32_t h) {
  if ((uint32_t)x >= (uint32_t)w || (uint32_t)y >= (uint32_t)h) return 0;  // außerhalb: schwarz
  return spread(s_fb[y * w + x]);
}

// Zeilen y0..y1 (Spalten ab x1, Breite ow) gedreht nach o schreiben
static void render_rows(int32_t x1, int32_t ow, int32_t y0, int32_t y1, lv_color_t *o) {
  int32_t w = s_drv->hor_res, h = s_drv->ver_res;
  for (int32_t y = y0; y <= y1; y++) {
    int32_t sx, sy;
    src_of(x1, y, w, h, &sx, &sy);
    // liegt die ganze Zeile innen? Dann ohne Randprüfung
    int32_t ex = sx + (ow - 1) * s_cos, ey = sy - (ow - 1) * s_sin;
    bool inside = (sx >> 16) >= 0 && (ex >> 16) >= 0 && (sx >> 16) < w - 1 && (ex >> 16) < w - 1 &&
                  (sy >> 16) >= 0 && (ey >> 16) >= 0 && (sy >> 16) < h - 1 && (ey >> 16) < h - 1;
    if (inside) {
      for (int32_t x = 0; x < ow; x++) {
        const lv_color_t *p = s_fb + (sy >> 16) * w + (sx >> 16);
        uint32_t fx = (sx >> 11) & 31, fy = (sy >> 11) & 31;
        uint32_t top = lerp(spread(p[0]), spread(p[1]), fx);
        uint32_t bot = lerp(spread(p[w]), spread(p[w + 1]), fx);
        *o++ = pack(lerp(top, bot, fy));
        sx += s_cos;
        sy -= s_sin;
      }
    } else {
      for (int32_t x = 0; x < ow; x++) {
        int32_t ix = sx >> 16, iy = sy >> 16;
        uint32_t fx = (sx >> 11) & 31, fy = (sy >> 11) & 31;
        uint32_t top = lerp(px_s(ix, iy, w, h), px_s(ix + 1, iy, w, h), fx);
        uint32_t bot = lerp(px_s(ix, iy + 1, w, h), px_s(ix + 1, iy + 1, w, h), fx);
        *o++ = pack(lerp(top, bot, fy));
        sx += s_cos;
        sy -= s_sin;
      }
    }
  }
}

// Bereich vorwärts drehen und einhüllen; auf 4er-Raster runden (SPD2010)
static void rotated_box(const lv_area_t *a, lv_area_t *r) {
  int32_t w = s_drv->hor_res, h = s_drv->ver_res;
  float cx = (w - 1) / 2.0f, cy = (h - 1) / 2.0f;
  float c = s_cos / 65536.0f, s = s_sin / 65536.0f;
  float xs[4] = { (float)a->x1, (float)a->x2, (float)a->x1, (float)a->x2 };
  float ys[4] = { (float)a->y1, (float)a->y1, (float)a->y2, (float)a->y2 };
  float x1 = 1e9f, y1 = 1e9f, x2 = -1e9f, y2 = -1e9f;
  for (int i = 0; i < 4; i++) {
    // Umkehrung von src_of: Ausgabe = R(+a) * (Quelle - Mitte) + Mitte
    float dx = xs[i] - cx, dy = ys[i] - cy;
    float ox = c * dx - s * dy + cx, oy = s * dx + c * dy + cy;
    if (ox < x1) x1 = ox;
    if (ox > x2) x2 = ox;
    if (oy < y1) y1 = oy;
    if (oy > y2) y2 = oy;
  }
  int32_t X1 = (int32_t)floorf(x1) - 2, Y1 = (int32_t)floorf(y1) - 2;
  int32_t X2 = (int32_t)ceilf(x2) + 2, Y2 = (int32_t)ceilf(y2) + 2;
  if (X1 < 0) X1 = 0;
  if (Y1 < 0) Y1 = 0;
  if (X2 > w - 1) X2 = w - 1;
  if (Y2 > h - 1) Y2 = h - 1;
  X1 &= ~3;
  Y1 &= ~3;
  X2 = (X2 | 3) > w - 1 ? w - 1 : (X2 | 3);
  Y2 = (Y2 | 3) > h - 1 ? h - 1 : (Y2 | 3);
  r->x1 = X1;
  r->y1 = Y1;
  r->x2 = X2;
  r->y2 = Y2;
}

// Geänderte Bereiche eines Bildes einzeln drehen: Ziffern links und
// Ringspitze rechts ergeben zwei kleine Blöcke statt einem ganzen Bild
#define MAX_REG 8
static lv_area_t s_reg[MAX_REG];
static int s_nreg = 0;
static bool s_reg_overflow = false;

static bool overlap_or_near(const lv_area_t *a, const lv_area_t *b) {
  return a->x1 <= b->x2 + 8 && b->x1 <= a->x2 + 8 && a->y1 <= b->y2 + 8 && b->y1 <= a->y2 + 8;
}

// ------------------------------------------------------------
//  Zweiter Kern rechnet mit: Kern 0 hat neben dem WLAN viel Leerlauf.
//  Jeder Streifen wird geteilt, oben rechnet die Anzeige, unten der Helfer.
// ------------------------------------------------------------
#ifdef ARDUINO
static TaskHandle_t s_worker = NULL;
static SemaphoreHandle_t s_go = NULL, s_done = NULL;
static volatile int32_t j_x1, j_ow, j_y0, j_y1;
static lv_color_t *volatile j_out;

static void worker(void *arg) {
  for (;;) {
    xSemaphoreTake(s_go, portMAX_DELAY);
    render_rows(j_x1, j_ow, j_y0, j_y1, j_out);
    xSemaphoreGive(s_done);
  }
}
#endif

static void render_rows_par(int32_t x1, int32_t ow, int32_t y0, int32_t y1, lv_color_t *o) {
#ifdef ARDUINO
  int32_t rows = y1 - y0 + 1;
  if (s_worker && rows >= 4) {
    int32_t mid = y0 + rows / 2;
    j_x1 = x1;
    j_ow = ow;
    j_y0 = mid;
    j_y1 = y1;
    j_out = o + (mid - y0) * ow;
    xSemaphoreGive(s_go);
    render_rows(x1, ow, y0, mid - 1, o);
    xSemaphoreTake(s_done, portMAX_DELAY);
    return;
  }
#endif
  render_rows(x1, ow, y0, y1, o);
}

static void wait_flush(lv_disp_drv_t *drv) {
  uint32_t t0 = lv_tick_get();
  while (drv->draw_buf->flushing && lv_tick_elaps(t0) < 100) {
  }
}

// Einen Ausgabebereich in Streifen rechnen und an den Treiber geben.
// Als Zwischenspeicher dienen die Original-Puffer des Treibers (interner,
// DMA-fähiger Speicher). So braucht der Treiber keine Hilfspuffer im
// knappen internen RAM, die sonst WLAN und TLS fehlen.
static void out_region(lv_disp_drv_t *drv, const lv_area_t *dst, bool last) {
  int32_t ow = dst->x2 - dst->x1 + 1;
  lv_color_t *bufs[2] = { (lv_color_t *)s_orig_buf->buf1,
                          (lv_color_t *)(s_orig_buf->buf2 ? s_orig_buf->buf2 : s_orig_buf->buf1) };
  int32_t rows = (int32_t)(s_orig_buf->size / ow);
  if (rows >= 8) rows &= ~3;  // auf das 4er-Raster des Displays
  if (rows < 1) rows = 1;
  int bi = 0;
  for (int32_t y = dst->y1; y <= dst->y2; y += rows) {
    int32_t y2 = y + rows - 1;
    if (y2 > dst->y2) y2 = dst->y2;
    lv_color_t *ob = bufs[bi];
    if (bufs[0] == bufs[1]) wait_flush(drv);  // nur ein Puffer: erst frei werden lassen
    render_rows_par(dst->x1, ow, y, y2, ob);
    wait_flush(drv);  // vorheriger Streifen fertig übertragen
    lv_area_t strip = { dst->x1, (lv_coord_t)y, dst->x2, (lv_coord_t)y2 };
    bool lst = last && y2 == dst->y2;
    drv->draw_buf->flushing = 1;
    drv->draw_buf->flushing_last = lst ? 1 : 0;
    s_orig_flush(drv, &strip, ob);  // ruft lv_disp_flush_ready()
    bi ^= 1;
  }
  if (!last) wait_flush(drv);
}

static void flush_rot(lv_disp_drv_t *drv, const lv_area_t *area_full, lv_color_t *color_p) {
  (void)color_p;  // direct_mode: liegt alles in s_fb
  // Im direct_mode meldet LVGL immer das ganze Bild; der wirklich neu
  // gezeichnete Bereich steht im clip_area des Zeichenkontexts
  const lv_area_t *area = (drv->draw_ctx && drv->draw_ctx->clip_area) ? drv->draw_ctx->clip_area : area_full;
#ifndef ARDUINO
  if (getenv("ROTDBG")) printf("ROT raw %d,%d %dx%d\n", (int)area->x1, (int)area->y1, (int)(area->x2 - area->x1 + 1), (int)(area->y2 - area->y1 + 1));
#endif
  if (s_nreg < MAX_REG) s_reg[s_nreg++] = *area;
  else s_reg_overflow = true;
  if (!s_have_dirty) {
    s_dirty = *area;
    s_have_dirty = true;
  } else {
    _lv_area_join(&s_dirty, &s_dirty, area);
  }
  if (!lv_disp_flush_is_last(drv)) {
    lv_disp_flush_ready(drv);
    return;
  }

  // Ausgabebereiche bestimmen (gedreht, sich berührende zusammengelegt)
  lv_area_t out[MAX_REG];
  int n = 0;
  if (s_reg_overflow) {
    rotated_box(&s_dirty, &out[0]);
    n = 1;
  } else {
    for (int i = 0; i < s_nreg; i++) rotated_box(&s_reg[i], &out[n++]);
    bool merged = true;
    while (merged) {
      merged = false;
      for (int i = 0; i < n && !merged; i++)
        for (int k = i + 1; k < n && !merged; k++)
          if (overlap_or_near(&out[i], &out[k])) {
            _lv_area_join(&out[i], &out[i], &out[k]);
            out[k] = out[--n];
            merged = true;
          }
    }
  }
  s_nreg = 0;
  s_reg_overflow = false;
  s_have_dirty = false;
  drv->draw_buf->flushing = 0;  // LVGL hat es für diesen Aufruf gesetzt; ab hier zählen unsere Streifen

  for (int i = 0; i < n; i++) {
#ifndef ARDUINO
    if (getenv("ROTDBG")) printf("ROT region %d %dx%d\n", i, (int)(out[i].x2 - out[i].x1 + 1), (int)(out[i].y2 - out[i].y1 + 1));
#endif
    out_region(drv, &out[i], i == n - 1);
  }
}

// Touch: physischer Punkt -> Punkt in der (ungedrehten) Oberfläche
static void read_rot(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  s_orig_read(drv, data);
  if (s_tenths == 0 || !s_drv) return;
  int32_t sx, sy;
  src_of(data->point.x, data->point.y, s_drv->hor_res, s_drv->ver_res, &sx, &sy);
  data->point.x = (lv_coord_t)((sx + 32768) >> 16);
  data->point.y = (lv_coord_t)((sy + 32768) >> 16);
}

static bool install() {
  lv_disp_t *disp = lv_disp_get_default();
  if (!disp) return false;
  s_drv = disp->driver;
  size_t n = (size_t)s_drv->hor_res * s_drv->ver_res;
  if (!s_drv->draw_buf || !s_drv->draw_buf->buf1) return false;
  s_fb = (lv_color_t *)big_alloc(n * sizeof(lv_color_t));
  if (!s_fb) return false;
#ifdef ARDUINO
  if (!s_worker) {
    s_go = xSemaphoreCreateBinary();
    s_done = xSemaphoreCreateBinary();
    if (s_go && s_done) xTaskCreatePinnedToCore(worker, "rot", 3072, NULL, 2, &s_worker, 0);
  }
#endif
  memset(s_fb, 0, n * sizeof(lv_color_t));
  lv_disp_draw_buf_init(&s_buf, s_fb, NULL, n);
  s_orig_flush = s_drv->flush_cb;
  s_orig_buf = s_drv->draw_buf;
  s_orig_direct = s_drv->direct_mode;
  s_orig_full = s_drv->full_refresh;
  s_drv->flush_cb = flush_rot;
  s_drv->draw_buf = &s_buf;
  s_drv->direct_mode = 1;
  s_drv->full_refresh = 0;
  for (lv_indev_t *in = lv_indev_get_next(NULL); in; in = lv_indev_get_next(in)) {
    if (in->driver->type == LV_INDEV_TYPE_POINTER && in->driver->read_cb != read_rot) {
      s_orig_read = in->driver->read_cb;
      s_indev_drv = in->driver;
      in->driver->read_cb = read_rot;
      break;
    }
  }
  s_installed = true;
  return true;
}

void disp_rot_suspend();

void disp_rot_set(int tenths) {
  if (tenths > DISP_ROT_MAX) tenths = DISP_ROT_MAX;
  if (tenths < -DISP_ROT_MAX) tenths = -DISP_ROT_MAX;
  if (tenths == 0 && s_installed) {  // gerade: Originaltreiber zurück, volle Geschwindigkeit
    disp_rot_suspend();
    s_tenths = 0;
    update_trig();
    return;
  }
  if (tenths != 0 && !s_installed && !install()) return;  // kein PSRAM: bleibt gerade
  s_tenths = tenths;
  update_trig();
  if (s_installed) lv_obj_invalidate(lv_scr_act());
  if (s_installed) lv_obj_invalidate(lv_layer_top());
}

int disp_rot_get() { return s_tenths; }

// Für das Online-Update: Originaltreiber zurück, Puffer freigeben (die
// Bildausgabe läuft dann wie ohne Drehung, das Bild steht kurz gerade)
void disp_rot_suspend() {
  if (!s_installed) return;
  s_drv->flush_cb = s_orig_flush;
  s_drv->draw_buf = s_orig_buf;
  s_drv->direct_mode = s_orig_direct;
  s_drv->full_refresh = s_orig_full;
  if (s_indev_drv && s_orig_read) s_indev_drv->read_cb = s_orig_read;
  free(s_fb);
  s_fb = NULL;
  s_installed = false;
  s_have_dirty = false;
  lv_obj_invalidate(lv_scr_act());
}

void disp_rot_resume() {
  int t = s_tenths;
  s_tenths = 0;
  disp_rot_set(t);
}
