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
static lv_color_t *s_out = NULL;  // gedrehter Ausschnitt für den Treiber
static lv_area_t s_dirty;         // geänderter Bereich dieses Bildes
static bool s_have_dirty = false;
static int32_t s_cos = 65536, s_sin = 0;  // 16.16
// Gespeicherter Originalzustand des Treibers (für disp_rot_suspend)
static lv_disp_draw_buf_t *s_orig_buf = NULL;
static uint32_t s_orig_direct = 0, s_orig_full = 0;
static lv_indev_drv_t *s_indev_drv = NULL;
// Große Änderungen (Seitenwechsel, Wischen) schnell ohne Glättung, danach
// einmal sauber geglättet nachzeichnen
#define FAST_AREA 80000  // ab ca. halbem Bild (Seitenwechsel, Wischen) schnell
static bool s_need_smooth = false;
static bool s_force_smooth = false;
static lv_timer_t *s_smooth_timer = NULL;

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

static inline lv_color_t px(int32_t x, int32_t y, int32_t w, int32_t h) {
  if (x < 0 || y < 0 || x >= w || y >= h) return lv_color_black();
  return s_fb[y * w + x];
}

// Schnell: nächster Nachbar (ein Lesezugriff pro Pixel)
static void render_fast(const lv_area_t *dst) {
  int32_t w = s_drv->hor_res, h = s_drv->ver_res;
  int32_t ow = dst->x2 - dst->x1 + 1;
  lv_color_t *o = s_out;
  lv_color_t black = lv_color_black();
  for (int32_t y = dst->y1; y <= dst->y2; y++) {
    int32_t sx, sy;
    src_of(dst->x1, y, w, h, &sx, &sy);
    sx += 32768;  // runden
    sy += 32768;
    for (int32_t x = 0; x < ow; x++) {
      int32_t ix = sx >> 16, iy = sy >> 16;
      *o++ = ((uint32_t)ix < (uint32_t)w && (uint32_t)iy < (uint32_t)h) ? s_fb[iy * w + ix] : black;
      sx += s_cos;
      sy -= s_sin;
    }
  }
}

// Nach einer schnellen Ausgabe: sobald Ruhe ist, einmal geglättet neu zeichnen
static void smooth_timer_cb(lv_timer_t *t) {
  s_smooth_timer = NULL;
#ifndef ARDUINO
  if (getenv("ROTDBG")) printf("ROT smooth timer\n");
#endif
  if (!s_installed || s_tenths == 0) return;
  s_force_smooth = true;
  lv_obj_invalidate(lv_scr_act());
  lv_obj_invalidate(lv_layer_top());
}

// Geglättet: bilinear (vier Lesezugriffe pro Pixel)
static void render(const lv_area_t *dst) {
  int32_t w = s_drv->hor_res, h = s_drv->ver_res;
  int32_t ow = dst->x2 - dst->x1 + 1;
  lv_color_t *o = s_out;
  for (int32_t y = dst->y1; y <= dst->y2; y++) {
    int32_t sx, sy;
    src_of(dst->x1, y, w, h, &sx, &sy);
    for (int32_t x = 0; x < ow; x++) {
      int32_t ix = sx >> 16, iy = sy >> 16;
      uint32_t fx = (sx >> 8) & 0xFF, fy = (sy >> 8) & 0xFF;  // Anteil 0..255
      lv_color_t c00 = px(ix, iy, w, h), c10 = px(ix + 1, iy, w, h);
      lv_color_t c01 = px(ix, iy + 1, w, h), c11 = px(ix + 1, iy + 1, w, h);
      uint32_t w00 = (256 - fx) * (256 - fy), w10 = fx * (256 - fy), w01 = (256 - fx) * fy, w11 = fx * fy;
      uint32_t r = (LV_COLOR_GET_R(c00) * w00 + LV_COLOR_GET_R(c10) * w10 + LV_COLOR_GET_R(c01) * w01 + LV_COLOR_GET_R(c11) * w11 + 32768) >> 16;
      uint32_t g = (LV_COLOR_GET_G(c00) * w00 + LV_COLOR_GET_G(c10) * w10 + LV_COLOR_GET_G(c01) * w01 + LV_COLOR_GET_G(c11) * w11 + 32768) >> 16;
      uint32_t b = (LV_COLOR_GET_B(c00) * w00 + LV_COLOR_GET_B(c10) * w10 + LV_COLOR_GET_B(c01) * w01 + LV_COLOR_GET_B(c11) * w11 + 32768) >> 16;
      lv_color_t c;
      c.full = 0;
      LV_COLOR_SET_R(c, r);
      LV_COLOR_SET_G(c, g);
      LV_COLOR_SET_B(c, b);
      *o++ = c;
      sx += s_cos;  // ein Pixel nach rechts
      sy -= s_sin;
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

// Einen Ausgabebereich rechnen und an den Treiber geben; bei weiteren
// Bereichen auf das Ende der Übertragung warten (s_out wird wiederverwendet)
static void out_region(lv_disp_drv_t *drv, const lv_area_t *dst, bool fast, bool last) {
  if (fast) render_fast(dst);
  else render(dst);
  drv->draw_buf->flushing = 1;
  drv->draw_buf->flushing_last = last ? 1 : 0;
  s_orig_flush(drv, dst, s_out);  // ruft lv_disp_flush_ready()
  if (!last) {
    uint32_t t0 = lv_tick_get();
    while (drv->draw_buf->flushing && lv_tick_elaps(t0) < 100) {
    }
  }
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

  bool big = false;
  for (int i = 0; i < n; i++) {
    int32_t a = (out[i].x2 - out[i].x1 + 1) * (out[i].y2 - out[i].y1 + 1);
    bool fast = a > FAST_AREA && !s_force_smooth;
    big = big || fast;
#ifndef ARDUINO
    if (getenv("ROTDBG")) printf("ROT region %d %dx%d %s\n", i, (int)(out[i].x2 - out[i].x1 + 1), (int)(out[i].y2 - out[i].y1 + 1), fast ? "schnell" : "glatt");
#endif
    out_region(drv, &out[i], fast, i == n - 1);
  }
  if (s_force_smooth) s_need_smooth = false;
  s_force_smooth = false;
  if (big) s_need_smooth = true;
  // nach 250 ms ohne große Änderung einmal geglättet nachzeichnen
  if (s_need_smooth && big) {
    if (s_smooth_timer) lv_timer_reset(s_smooth_timer);
    else {
      s_smooth_timer = lv_timer_create(smooth_timer_cb, 250, NULL);
      lv_timer_set_repeat_count(s_smooth_timer, 1);
    }
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
  s_fb = (lv_color_t *)big_alloc(n * sizeof(lv_color_t));
  s_out = (lv_color_t *)big_alloc(n * sizeof(lv_color_t));
  if (!s_fb || !s_out) {
    free(s_fb);
    free(s_out);
    s_fb = s_out = NULL;
    return false;
  }
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
  if (s_smooth_timer) {
    lv_timer_del(s_smooth_timer);
    s_smooth_timer = NULL;
  }
  s_drv->flush_cb = s_orig_flush;
  s_drv->draw_buf = s_orig_buf;
  s_drv->direct_mode = s_orig_direct;
  s_drv->full_refresh = s_orig_full;
  if (s_indev_drv && s_orig_read) s_indev_drv->read_cb = s_orig_read;
  free(s_fb);
  free(s_out);
  s_fb = s_out = NULL;
  s_installed = false;
  s_have_dirty = false;
  lv_obj_invalidate(lv_scr_act());
}

void disp_rot_resume() {
  int t = s_tenths;
  s_tenths = 0;
  disp_rot_set(t);
}
