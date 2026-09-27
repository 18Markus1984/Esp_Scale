// ============================================================
//  Küchentimer: bis zu drei Timer, laufen im Hintergrund weiter
//  (tools.cpp). Abgelaufene Timer klingeln auf jeder Seite.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "sound.h"
#include "tools.h"
#include <stdio.h>

static int t_set_min[TIMER_COUNT] = { 5, 10, 15 };  // zuletzt eingestellte Dauer
static int t_edit = 0;

static lv_obj_t *page_timer_edit_create();

// ------------------------------------------------------------
//  Übersicht
// ------------------------------------------------------------
static lv_obj_t *tl_val[TIMER_COUNT];

static void tl_timer_cb(lv_timer_t *t) {
  for (int i = 0; i < TIMER_COUNT; i++) {
    char b[24], v[16];
    if (timer_running(i)) {
      timer_fmt(v, sizeof(v), timer_left(i));
      snprintf(b, sizeof(b), "%s ›", v);
    } else {
      snprintf(b, sizeof(b), T("%d min ›"), t_set_min[i]);
    }
    ui_label_update(tl_val[i], b);
    lv_obj_set_style_text_color(tl_val[i], timer_running(i) ? C_ACCENT : C_MUTED, 0);
  }
}

static void tl_row_cb(lv_event_t *e) {
  t_edit = (int)(intptr_t)lv_event_get_user_data(e);
  ui_switch_page(page_timer_edit_create());
}

lv_obj_t *page_timer_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Timer", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);

  for (int i = 0; i < TIMER_COUNT; i++) {
    lv_obj_t *row = ui_box(s);
    lv_obj_set_size(row, 280, 58);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -62 + i * 62);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, tl_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *ic = ui_label(row, ICON_TIMER, &font_icons_26, C_FAINT);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *l = ui_label(row, timer_name(i), &font_sg_18, C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 42, 0);
    tl_val[i] = ui_label(row, "", &font_sg_24, C_MUTED);
    lv_obj_align(tl_val[i], LV_ALIGN_RIGHT_MID, -4, 0);
  }

  lv_obj_t *h = ui_label(s, "Laufen im Hintergrund weiter", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 144);

  ui_page_timer(s, tl_timer_cb, 500);
  tl_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Einstellen / laufender Timer
// ------------------------------------------------------------
static lv_obj_t *te_time, *te_set, *te_run;
static const int PRESETS[5] = { 1, 5, 10, 30, 60 };

static void te_show() {
  char b[16];
  bool run = timer_running(t_edit);
  if (run) timer_fmt(b, sizeof(b), timer_left(t_edit));
  else timer_fmt(b, sizeof(b), t_set_min[t_edit] * 60);
  ui_label_update(te_time, b);
  if (run) {
    lv_obj_add_flag(te_set, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(te_run, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(te_set, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(te_run, LV_OBJ_FLAG_HIDDEN);
  }
}

static void te_timer_cb(lv_timer_t *t) {
  te_show();
}

static void te_adj_cb(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  if (timer_running(t_edit)) {
    timer_add(t_edit, d * 60);
  } else {
    t_set_min[t_edit] += d;
    if (t_set_min[t_edit] < 1) t_set_min[t_edit] = 1;
    if (t_set_min[t_edit] > 600) t_set_min[t_edit] = 600;
  }
  sound_play(SND_CLICK);
  te_show();
}

static void te_preset_cb(lv_event_t *e) {
  t_set_min[t_edit] = (int)(intptr_t)lv_event_get_user_data(e);
  sound_play(SND_CLICK);
  te_show();
}

static lv_obj_t *page_timer_edit_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, timer_name(t_edit), &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);

  te_time = ui_label(s, "", &font_sg_80, C_TEXT);
  lv_obj_align(te_time, LV_ALIGN_CENTER, 0, -64);

  lv_obj_t *minus = ui_round_btn(s, "−", NULL);
  lv_obj_align(minus, LV_ALIGN_CENTER, -70, 18);
  lv_obj_add_event_cb(minus, te_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
  lv_obj_t *plus = ui_round_btn(s, "+", NULL);
  lv_obj_align(plus, LV_ALIGN_CENTER, 70, 18);
  lv_obj_add_event_cb(plus, te_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
  lv_obj_t *u = ui_label(s, "1 min", &font_sg_14, C_FAINT);
  lv_obj_align(u, LV_ALIGN_CENTER, 0, 18);

  // nicht laufend: Vorwahl + Start
  te_set = ui_box(s);
  lv_obj_set_size(te_set, 360, 120);
  lv_obj_align(te_set, LV_ALIGN_CENTER, 0, 118);
  lv_obj_t *row = ui_box(te_set);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
  for (int i = 0; i < 5; i++) {
    char b[8];
    snprintf(b, sizeof(b), "%d", PRESETS[i]);
    lv_obj_t *c = ui_btn(row, b, BTN_NORMAL);
    lv_obj_set_height(c, 40);
    lv_obj_set_style_pad_hor(c, 12, 0);
    lv_obj_add_event_cb(c, te_preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PRESETS[i]);
  }
  lv_obj_t *start = ui_btn(te_set, "Start", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(start, 30, 0);
  lv_obj_align(start, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_add_event_cb(start, [](lv_event_t *e) {
    timer_start(t_edit, (uint32_t)t_set_min[t_edit] * 60, NULL);
    sound_play(SND_SAVE);
    te_show();
  }, LV_EVENT_CLICKED, NULL);

  // laufend: Stopp + zurück zur Übersicht
  te_run = ui_box(s);
  lv_obj_set_size(te_run, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(te_run, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(te_run, 12, 0);
  lv_obj_align(te_run, LV_ALIGN_CENTER, 0, 100);
  lv_obj_t *stop = ui_btn(te_run, "Stopp", BTN_WARN);
  lv_obj_add_event_cb(stop, [](lv_event_t *e) {
    timer_stop(t_edit);
    sound_play(SND_CLICK);
    te_show();
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ok = ui_btn(te_run, "Übersicht", BTN_NORMAL);
  lv_obj_add_event_cb(ok, [](lv_event_t *e) { ui_switch_page(page_timer_create()); }, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, te_timer_cb, 250);
  te_show();
  return s;
}
