// ============================================================
//  Feinausrichtung des Displays (siehe disp_rot.h)
// ============================================================
#include "disp_rot.h"
#include <lvgl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
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

static void flush_rot(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  (void)color_p;  // direct_mode: liegt alles in s_fb
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
  lv_area_t dst;
  if (s_tenths == 0) {
    // ohne Drehung nur umkopieren (der Treiber erwartet einen zusammenhängenden Block)
    dst = s_dirty;
    int32_t w = drv->hor_res, ow = dst.x2 - dst.x1 + 1;
    for (int32_t y = dst.y1; y <= dst.y2; y++)
      memcpy(s_out + (y - dst.y1) * ow, s_fb + y * w + dst.x1, ow * sizeof(lv_color_t));
  } else {
    rotated_box(&s_dirty, &dst);
    render(&dst);
  }
  s_have_dirty = false;
  s_orig_flush(drv, &dst, s_out);  // ruft lv_disp_flush_ready()
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
  s_drv->flush_cb = flush_rot;
  s_drv->draw_buf = &s_buf;
  s_drv->direct_mode = 1;
  s_drv->full_refresh = 0;
  for (lv_indev_t *in = lv_indev_get_next(NULL); in; in = lv_indev_get_next(in)) {
    if (in->driver->type == LV_INDEV_TYPE_POINTER && in->driver->read_cb != read_rot) {
      s_orig_read = in->driver->read_cb;
      in->driver->read_cb = read_rot;
      break;
    }
  }
  s_installed = true;
  return true;
}

void disp_rot_set(int tenths) {
  if (tenths > DISP_ROT_MAX) tenths = DISP_ROT_MAX;
  if (tenths < -DISP_ROT_MAX) tenths = -DISP_ROT_MAX;
  if (tenths != 0 && !s_installed && !install()) return;  // kein PSRAM: bleibt gerade
  s_tenths = tenths;
  update_trig();
  if (s_installed) lv_obj_invalidate(lv_scr_act());
  if (s_installed) lv_obj_invalidate(lv_layer_top());
}

int disp_rot_get() { return s_tenths; }
