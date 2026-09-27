#include "ui_widgets.h"
#include "ui_theme.h"
#include "hal.h"
#include "settings.h"
#include "config.h"
#include <math.h>

// ------------------------------------------------------------
//  Kurvenliste
// ------------------------------------------------------------
#define ITEM_W 290
#define ITEM_H 64
#define FADE_DIST 200  // ab diesem Abstand zur Mitte kaum noch sichtbar
// Maximal 32 Einträge pro Liste

static bool s_force = false;  // beim Anlegen alles einmal setzen

static void list_restyle(lv_obj_t *cont) {
  lv_area_t ca;
  lv_obj_get_coords(cont, &ca);
  lv_coord_t center_y = ca.y1 + lv_area_get_height(&ca) / 2;

  uint32_t n = lv_obj_get_child_cnt(cont);
  lv_obj_t *best = NULL;
  lv_coord_t best_d = LV_COORD_MAX;

  lv_coord_t dist[32];
  for (uint32_t i = 0; i < n && i < 32; i++) {
    lv_obj_t *item = lv_obj_get_child(cont, i);
    lv_area_t a;
    lv_obj_get_coords(item, &a);
    lv_coord_t d = LV_ABS((a.y1 + lv_area_get_height(&a) / 2) - center_y);
    dist[i] = d;
    if (d < best_d) { best_d = d; best = item; }
  }

  // Beim Scrollen kommt dieses Ereignis sehr oft. Deshalb nur ändern, was
  // sich wirklich ändert: Schrift, Hintergrund und Untertitel nur beim Wechsel
  // des ausgewählten Eintrags, die Farbe nur, wenn sie anders ist.
  for (uint32_t i = 0; i < n && i < 32; i++) {
    lv_obj_t *item = lv_obj_get_child(cont, i);
    lv_obj_t *title = lv_obj_get_child(item, 0);
    lv_obj_t *sub = lv_obj_get_child_cnt(item) > 1 ? lv_obj_get_child(item, 1) : NULL;
    bool sel = (item == best);
    bool was = lv_obj_has_state(item, LV_STATE_CHECKED);

    if (sel != was || s_force) {
      if (sel) lv_obj_add_state(item, LV_STATE_CHECKED);
      else lv_obj_clear_state(item, LV_STATE_CHECKED);
      lv_obj_set_style_bg_opa(item, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
      lv_obj_set_style_text_font(title, sel ? &font_sg_24 : &font_sg_18, 0);
      if (sub) {
        if (sel) lv_obj_clear_flag(sub, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(sub, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, sel ? -9 : 0);
      }
    }

    // weiter weg von der Mitte = dunkler -> wirkt wie eine Walze
    lv_coord_t dd = dist[i] > FADE_DIST ? FADE_DIST : dist[i];
    lv_opa_t mix = (lv_opa_t)(255 - (dd * 200) / FADE_DIST);
    mix &= 0xF0;  // in Stufen, damit nicht jeder Pixel Bewegung neu zeichnet
    lv_color_t c = sel ? C_ON_ACCENT : lv_color_mix(C_TEXT2, C_BG, mix);
    if (s_force || lv_obj_get_style_text_color(title, 0).full != c.full) {
      lv_obj_set_style_text_color(title, c, 0);
      // Symbol (letztes Kind des Eintrags) in derselben Farbe
      uint32_t n_child = lv_obj_get_child_cnt(item);
      if (n_child > 1) {
        lv_obj_t *last = lv_obj_get_child(item, n_child - 1);
        if (last != title) lv_obj_set_style_text_color(last, c, 0);
      }
    }
  }
}

static void list_scroll_cb(lv_event_t *e) {
  list_restyle(lv_event_get_target(e));
}

static void item_click_cb(lv_event_t *e) {
  lv_obj_t *item = lv_event_get_current_target(e);
  lv_obj_t *cont = lv_obj_get_parent(item);
  if (lv_obj_has_state(item, LV_STATE_CHECKED)) {
    ui_list_cb_t cb = (ui_list_cb_t)lv_obj_get_user_data(cont);
    if (cb) cb((int)(intptr_t)lv_obj_get_user_data(item));
  } else {
    lv_obj_scroll_to_view(item, LV_ANIM_ON);
  }
}

lv_obj_t *ui_curved_list(lv_obj_t *parent, const ui_list_item_t *items, int count, ui_list_cb_t cb) {
  lv_obj_t *cont = ui_box(parent);
  lv_obj_set_size(cont, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_center(cont);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(cont, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_CENTER);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(cont, 10, 0);
  lv_obj_set_style_pad_top(cont, (SCREEN_SIZE - ITEM_H) / 2, 0);
  lv_obj_set_style_pad_bottom(cont, (SCREEN_SIZE - ITEM_H) / 2, 0);
  lv_obj_set_user_data(cont, (void *)cb);

  for (int i = 0; i < count; i++) {
    lv_obj_t *item = ui_box(cont);
    lv_obj_set_size(item, ITEM_W, ITEM_H);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_set_style_radius(item, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(item, C_ACCENT, 0);
    lv_obj_set_user_data(item, (void *)(intptr_t)i);
    lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *title = ui_label(item, items[i].title, &font_sg_18, C_TEXT2);
    lv_obj_align(title, LV_ALIGN_CENTER, items[i].icon ? 14 : 0, 0);
    if (items[i].sub) {
      lv_obj_t *sub = ui_label(item, items[i].sub, &font_sg_14, lv_color_hex(0x1D4D38));
      lv_obj_align(sub, LV_ALIGN_CENTER, 0, 15);
      lv_obj_add_flag(sub, LV_OBJ_FLAG_HIDDEN);
    }
    if (items[i].icon) {  // Symbol links neben dem Titel
      lv_obj_t *ic = ui_label(item, items[i].icon, &font_icons_26, C_TEXT2);
      lv_obj_align(ic, LV_ALIGN_LEFT_MID, 26, 0);
    }

  }

  lv_obj_add_event_cb(cont, list_scroll_cb, LV_EVENT_SCROLL, NULL);
  lv_obj_update_layout(cont);
  s_force = true;
  list_restyle(cont);
  s_force = false;
  return cont;
}

// ------------------------------------------------------------
//  Libelle
// ------------------------------------------------------------
lv_obj_t *ui_bubble_create(lv_obj_t *parent, int size) {
  lv_obj_t *c = ui_box(parent);
  lv_obj_set_size(c, size, size);
  lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_border_color(c, C_BORDER, 0);

  lv_obj_t *h = ui_box(c);
  lv_obj_set_size(h, size - 8, 2);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(h, C_TRACK, 0);
  lv_obj_center(h);

  lv_obj_t *v = ui_box(c);
  lv_obj_set_size(v, 2, size - 8);
  lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(v, C_TRACK, 0);
  lv_obj_center(v);

  lv_obj_t *ring = ui_box(c);
  lv_obj_set_size(ring, size / 4, size / 4);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(ring, 2, 0);
  lv_obj_set_style_border_color(ring, C_BORDER, 0);
  lv_obj_center(ring);

  int dot_size = size * 18 / 100;
  lv_obj_t *dot = ui_box(c);
  lv_obj_set_size(dot, dot_size, dot_size);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dot, C_ACCENT, 0);
  lv_obj_center(dot);

  lv_obj_set_user_data(c, dot);
  return c;
}

void ui_bubble_set(lv_obj_t *bubble, float dx_deg, float dy_deg, lv_color_t color) {
  lv_obj_t *dot = (lv_obj_t *)lv_obj_get_user_data(bubble);
  float size = (float)lv_obj_get_style_width(bubble, 0);
  float dot_size = (float)lv_obj_get_style_width(dot, 0);
  float max_r = (size - dot_size) / 2.0f - 4.0f;
  float px_per_deg = max_r / 6.0f;

  float x = dx_deg * px_per_deg;
  float y = dy_deg * px_per_deg;
  float r = sqrtf(x * x + y * y);
  if (r > max_r) {
    x *= max_r / r;
    y *= max_r / r;
  }
  lv_obj_align(dot, LV_ALIGN_CENTER, (lv_coord_t)x, (lv_coord_t)y);
  lv_obj_set_style_bg_color(dot, color, 0);
}

// Einbaulage berücksichtigen: Achsen so drehen, dass "vorne" am Display
// auch vorne an der Waage ist
bool ui_read_tilt_raw(float *dx_deg, float *dy_deg) {
  float x, y, z;
  if (!hal_accel(&x, &y, &z)) return false;
  float g = sqrtf(x * x + y * y + z * z);
  if (g < 0.01f) return false;
  float rx = x, ry = y;
  switch (DISPLAY_MOUNT) {
    case 1: rx = y;  ry = -x; break;
    case 2: rx = -x; ry = -y; break;
    case 3: rx = -y; ry = x;  break;
    default: break;
  }
  const float RAD2DEG = 57.29578f;
  *dx_deg = atan2f(rx, fabsf(z)) * RAD2DEG;
  *dy_deg = atan2f(ry, fabsf(z)) * RAD2DEG;
  return true;
}

bool ui_read_tilt(float *tilt_deg, float *dx_deg, float *dy_deg) {
  if (!ui_read_tilt_raw(dx_deg, dy_deg)) return false;
  *dx_deg -= g_set.lvl_off_x;  // Nullpunkt aus "Libelle kalibrieren"
  *dy_deg -= g_set.lvl_off_y;
  *tilt_deg = sqrtf(*dx_deg * *dx_deg + *dy_deg * *dy_deg);
  return true;
}
