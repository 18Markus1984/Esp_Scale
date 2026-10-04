#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

lv_obj_t *ui_screen_create() {
  lv_obj_t *s = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s, C_BG, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(s, C_TEXT, 0);
  lv_obj_set_style_text_font(s, &font_sg_18, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

lv_obj_t *ui_box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

lv_obj_t *ui_btn(lv_obj_t *parent, const char *txt, ui_btn_kind_t kind) {
  lv_color_t bg = C_SURFACE;
  lv_color_t fg = C_TEXT;
  if (kind == BTN_PRIMARY) { bg = C_PRIMARY; fg = C_ON_PRIMARY; }
  if (kind == BTN_WARN)    { bg = C_SURFACE; fg = C_WARN; }  // nur Rahmen und Schrift gelb

  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_height(b, 52);
  lv_obj_set_width(b, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(b, 24, 0);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, kind == BTN_PRIMARY ? 0 : 2, 0);
  lv_obj_set_style_border_color(b, kind == BTN_WARN ? C_WARN : C_BORDER, 0);
  // gedrückt: weiße Knöpfe werden grauer, dunkle heller
  lv_obj_set_style_bg_color(b, kind == BTN_PRIMARY ? lv_color_hex(0xC9C9C5) : lv_color_lighten(bg, LV_OPA_20), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_40, LV_STATE_DISABLED);

  lv_obj_t *l = ui_label(b, txt, &font_sg_18, fg);
  lv_obj_center(l);
  return b;
}

lv_obj_t *ui_chip(lv_obj_t *parent, const char *txt, lv_color_t color) {
  lv_obj_t *c = ui_label(parent, txt, &font_sg_14, color);
  lv_obj_set_style_pad_hor(c, 14, 0);
  lv_obj_set_style_pad_ver(c, 5, 0);
  lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_border_color(c, color, 0);
  lv_obj_set_style_border_opa(c, LV_OPA_60, 0);
  return c;
}

void ui_chip_set(lv_obj_t *chip, const char *txt, lv_color_t color, bool filled) {
  ui_label_update(chip, txt);
  lv_obj_set_style_border_color(chip, color, 0);
  // gefüllt (Toast): normale Meldungen weiß wie die Bedienfläche, Warnungen farbig
  lv_color_t fill = (filled && color.full == C_ACCENT.full) ? C_PRIMARY : color;
  lv_obj_set_style_bg_color(chip, fill, 0);
  lv_obj_set_style_bg_opa(chip, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_text_color(chip, filled ? C_ON_PRIMARY : color, 0);
}

lv_obj_t *ui_ring(lv_obj_t *parent, int size) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_set_size(a, size, size);
  lv_obj_center(a);
  lv_arc_set_rotation(a, 270);
  lv_arc_set_bg_angles(a, 0, 360);
  lv_arc_set_range(a, 0, 1000);
  lv_arc_set_value(a, 0);
  lv_obj_remove_style(a, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(a, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, C_TRACK, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, C_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
  return a;
}

lv_obj_t *ui_page_dots(lv_obj_t *parent, int count, int active) {
  lv_obj_t *row = ui_box(parent);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  for (int i = 0; i < count; i++) {
    lv_obj_t *d = ui_box(row);
    lv_obj_set_size(d, 8, 8);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, i == active ? C_TEXT : C_BORDER, 0);
  }
  return row;
}

lv_obj_t *ui_round_btn(lv_obj_t *parent, const char *txt, lv_event_cb_t cb) {
  lv_obj_t *b = ui_btn(parent, txt, BTN_NORMAL);
  lv_obj_set_size(b, 60, 60);
  lv_obj_set_style_pad_hor(b, 0, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &font_sg_34, 0);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
  return b;
}

// Jeder Seiten-Timer gehört zu genau einem Screen. Beim Löschen des Screens
// wird genau dieser Timer gelöscht - auch wenn schon eine neue Seite
// desselben Typs (mit neuem Timer) angezeigt wird.
static void page_timer_delete_cb(lv_event_t *e) {
  lv_timer_del((lv_timer_t *)lv_event_get_user_data(e));
}

lv_timer_t *ui_page_timer(lv_obj_t *page, lv_timer_cb_t cb, uint32_t period_ms) {
  lv_timer_t *t = lv_timer_create(cb, period_ms, NULL);
  lv_obj_add_event_cb(page, page_timer_delete_cb, LV_EVENT_DELETE, t);
  return t;
}

// Der laufende Timer wird im user_data des Toasts gemerkt,
// damit er beim Löschen der Seite mit aufgeräumt wird.
static void toast_timer_cb(lv_timer_t *t) {
  lv_obj_t *c = (lv_obj_t *)t->user_data;
  lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_user_data(c, NULL);  // Timer löscht sich selbst (repeat_count = 1)
}

static void toast_delete_cb(lv_event_t *e) {
  lv_timer_t *t = (lv_timer_t *)lv_obj_get_user_data(lv_event_get_target(e));
  if (t) lv_timer_del(t);
}

lv_obj_t *ui_toast_create(lv_obj_t *parent) {
  lv_obj_t *c = ui_chip(parent, "", C_ACCENT);
  lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_user_data(c, NULL);
  lv_obj_add_event_cb(c, toast_delete_cb, LV_EVENT_DELETE, NULL);
  return c;
}

void ui_toast_show(lv_obj_t *toast, const char *txt, lv_color_t color) {
  ui_chip_set(toast, txt, color, true);
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lv_timer_t *t = (lv_timer_t *)lv_obj_get_user_data(toast);
  if (t) {
    lv_timer_reset(t);
  } else {
    t = lv_timer_create(toast_timer_cb, 2000, toast);
    lv_timer_set_repeat_count(t, 1);
    lv_obj_set_user_data(toast, t);
  }
}

void ui_label_update(lv_obj_t *label, const char *txt) {
  txt = T(txt);  // mit dem übersetzten Text vergleichen, sonst wird ständig neu gezeichnet
  const char *old = lv_label_get_text(label);
  if (old == NULL || strcmp(old, txt) != 0) lv_label_set_text(label, txt);
}

void ui_fmt_weight(char *buf, size_t len, float grams) {
  ui_fmt_1dec(buf, len, grams);
}

void ui_fmt_1dec(char *buf, size_t len, float value) {
  if (value > -0.05f && value < 0.05f) value = 0.0f;  // kein "-0,0"
  snprintf(buf, len, "%.1f", value);
  for (char *p = buf; *p; p++) {
    if (*p == '.') *p = ',';
  }
}
