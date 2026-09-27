// ============================================================
//  System -> Protokoll (siehe Design-Sheet)
//   Tagesansicht: Uhrzeit und Gewicht, neueste zuerst
//   Ältere Tage:  Kurvenliste aller Tagesdateien
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "storage.h"
#include "data.h"
#include <stdio.h>
#include <string.h>

#define DAYS_MAX 30
#define ROWS_MAX 50

static char s_days[DAYS_MAX][11];
static int s_day_count;
static char s_day[11];  // angezeigter Tag

static lv_obj_t *page_day_create();
static lv_obj_t *page_days_create();

// "2026-09-18" -> "18.09."
static void short_date(const char *day, char *out, int len) {
  snprintf(out, len, "%.2s.%.2s.", day + 8, day + 5);
}

// ------------------------------------------------------------
//  Tagesansicht
// ------------------------------------------------------------
static void other_days_cb(lv_event_t *e) {
  ui_switch_page(page_days_create());
}

static lv_obj_t *page_day_create() {
  lv_obj_t *s = ui_screen_create();
  char b[48], today[11];
  log_today(today);

  if (strcmp(s_day, today) == 0) {
    char d[8];
    short_date(s_day, d, sizeof(d));
    snprintf(b, sizeof(b), T("Heute · %s"), d);
  } else {
    snprintf(b, sizeof(b), "%.2s.%.2s.%.4s", s_day + 8, s_day + 5, s_day);
  }
  lv_obj_t *t = ui_label(s, b, &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 280, 170);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -14);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  static log_entry_t rows[ROWS_MAX];
  int total = 0;
  int n = 0;
  if (!storage_ok()) {
    ui_label(list, "Keine SD-Karte", &font_sg_18, C_WARN);
  } else if (s_day[0]) {
    n = log_read(s_day, rows, ROWS_MAX, &total);
  }
  if (storage_ok() && n == 0) ui_label(list, "Noch keine Wägungen", &font_sg_18, C_MUTED);

  for (int i = 0; i < n; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 280, 38);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);

    char tm[6];
    snprintf(tm, sizeof(tm), "%.5s", rows[i].time);  // HH:MM
    lv_obj_t *l = ui_label(row, tm, &font_sg_18, C_MUTED);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

    char w[16];
    unit_fmt(w, sizeof(w), rows[i].value, unit_index(rows[i].unit));
    snprintf(b, sizeof(b), "%s %s", w, rows[i].unit);
    lv_obj_t *r = ui_label(row, b, &font_sg_18, C_TEXT);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);

    if (rows[i].note[0]) {  // Topf, Rezept, Modus-Ergebnis
      lv_obj_t *n = ui_label(row, rows[i].note, &font_sg_14, C_FAINT);
      lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
      lv_obj_set_width(n, 110);
      lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_LEFT, 0);
      lv_obj_align(n, LV_ALIGN_LEFT_MID, 58, 0);
    }
  }

  snprintf(b, sizeof(b), total == 1 ? T("%d Wägung") : T("%d Wägungen"), total);
  lv_obj_t *cnt = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(cnt, LV_ALIGN_CENTER, 0, 88);

  if (s_day[0]) {
    snprintf(b, sizeof(b), "%s.txt", s_day);
    lv_obj_t *f = ui_label(s, b, &font_sg_14, C_FAINT);
    lv_obj_align(f, LV_ALIGN_CENTER, 0, 112);
  }

  lv_obj_t *btn = ui_btn(s, "Andere Tage", BTN_NORMAL);
  lv_obj_set_height(btn, 44);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 150);
  lv_obj_add_event_cb(btn, other_days_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

// ------------------------------------------------------------
//  Ältere Tage
// ------------------------------------------------------------
static void day_pick_cb(int index) {
  strcpy(s_day, s_days[index]);
  ui_switch_page(page_day_create());
}

static lv_obj_t *page_days_create() {
  lv_obj_t *s = ui_screen_create();
  s_day_count = log_days(s_days, DAYS_MAX);

  if (s_day_count == 0) {
    lv_obj_t *l = ui_label(s, storage_ok() ? "Noch keine Tage\nim Protokoll" : "Keine SD-Karte",
                           &font_sg_18, C_MUTED);
    lv_obj_center(l);
    return s;
  }

  char today[11];
  log_today(today);
  static char titles[DAYS_MAX][16];
  static char subs[DAYS_MAX][20];
  static ui_list_item_t items[DAYS_MAX];
  static log_entry_t dummy[1];
  for (int i = 0; i < s_day_count; i++) {
    char d[8];
    short_date(s_days[i], d, sizeof(d));
    if (strcmp(s_days[i], today) == 0) snprintf(titles[i], sizeof(titles[i]), T("Heute"));
    else snprintf(titles[i], sizeof(titles[i]), "%s", d);
    int total = 0;
    log_read(s_days[i], dummy, 1, &total);
    snprintf(subs[i], sizeof(subs[i]), total == 1 ? T("%d Wägung") : T("%d Wägungen"), total);
    items[i].title = titles[i];
    items[i].sub = subs[i];
  }
  ui_curved_list(s, items, s_day_count, day_pick_cb);

  lv_obj_t *t = ui_label(s, "Protokoll · Tage", &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return s;
}

// ------------------------------------------------------------
lv_obj_t *page_protokoll_create() {
  log_today(s_day);  // leer, wenn die Uhr noch nicht gestellt ist
  if (!s_day[0]) return page_days_create();
  return page_day_create();
}
