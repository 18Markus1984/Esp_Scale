// ============================================================
//  System -> Akku
//  Zeigt den Ladestand, den Verlauf der letzten Stunden, den
//  Verbrauch pro Stunde und die geschätzte Restlaufzeit.
//  Am Ladegerät steht statt des Prozentwerts "lädt" bzw. "voll",
//  weil die gemessene Spannung dann vom Ladegerät kommt.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "batt.h"
#include "hal.h"
#include "config.h"
#include "sound.h"
#include <stdio.h>
#include <math.h>

#define BARS 24  // so viele Balken zeigt der Verlauf

static lv_obj_t *a_ring, *a_val, *a_unit, *a_state, *a_test, *a_info, *a_bars[BARS], *a_scale;

static void a_timer_cb(lv_timer_t *t) {
  int pct = hal_battery_percent();
  bat_state_t st = hal_battery_state();
  char b[64];

  if (st == BAT_NONE) {
    ui_label_update(a_val, "–");
    ui_label_update(a_unit, "");
    ui_label_update(a_state, "kein Akku");
    ui_label_update(a_info, "läuft über USB");
    lv_arc_set_value(a_ring, 0);
    return;
  }

  bool usb = st == BAT_CHARGING || st == BAT_FULL;
  if (usb) {
    // Am Ladegerät kommt die gemessene Spannung vom Ladegerät, nicht vom Akku:
    // dann lieber die Spannung zeigen als einen Prozentwert, der nicht stimmt.
    snprintf(b, sizeof(b), "%.2f", hal_battery_volts());
    for (char *c = b; *c; c++)
      if (*c == '.') *c = ',';
  } else {
    snprintf(b, sizeof(b), "%d", pct);
  }
  ui_label_update(a_val, b);
  ui_label_update(a_unit, usb ? "V" : "%");
  lv_arc_set_value(a_ring, usb ? 1000 : pct * 10);

  bool low = hal_battery_volts() < BAT_WARN_V && st == BAT_DISCHARGE;
  lv_color_t c = st == BAT_CHARGING ? C_WARN
                                    : (st == BAT_FULL ? C_ACCENT : ((pct < 20 || low) ? C_DANGER : C_TEXT));
  lv_obj_set_style_text_color(a_val, c, 0);
  lv_obj_set_style_arc_color(a_ring, c, LV_PART_INDICATOR);

  if (st == BAT_CHARGING) ui_label_update(a_state, "lädt am USB");
  else if (st == BAT_FULL) ui_label_update(a_state, "voll geladen");
  else ui_label_update(a_state, low ? "fast leer – laden!" : "Akkubetrieb");
  lv_obj_set_style_text_color(a_state, c, 0);

  float per = batt_per_hour();
  float left = batt_hours_left();
  if (usb) {
    snprintf(b, sizeof(b), T("Ladestand erst ohne Kabel messbar"));
  } else if (per > 0.2f && left > 0) {
    snprintf(b, sizeof(b), T("%.1f %%/h · noch ca. %.1f h"), per, left);
  } else {
    snprintf(b, sizeof(b), T("%.2f V · Verbrauch wird gemessen"), hal_battery_volts());
  }
  if (batt_test_running()) {
    snprintf(b, sizeof(b), T("Test · %.1f h · von %.2f auf %.2f V · Auto-Aus pausiert"), batt_test_hours(),
             batt_test_v_start(), hal_battery_volts());
    lv_obj_set_style_text_color(a_info, C_WARN, 0);
  } else {
    lv_obj_set_style_text_color(a_info, C_MUTED, 0);
  }
  ui_label_update(a_info, b);
  lv_label_set_text(lv_obj_get_child(a_test, 0), batt_test_running() ? "Test beenden" : "Entladetest");

  // Verlauf: die letzten BARS Messpunkte (alle 5 min einer)
  int n = batt_count();
  for (int i = 0; i < BARS; i++) {
    int idx = n - BARS + i;
    int v = idx >= 0 ? batt_percent_at(idx) : -1;
    lv_coord_t h = v < 0 ? 2 : (lv_coord_t)(4 + v * 56 / 100);
    lv_obj_set_height(a_bars[i], h);
    lv_obj_set_style_bg_color(a_bars[i], v < 0 ? C_TRACK : (v < 20 ? C_DANGER : C_ACCENT), 0);
    lv_obj_set_style_bg_opa(a_bars[i], v < 0 ? LV_OPA_30 : LV_OPA_COVER, 0);
  }
  int have = n < BARS ? n : BARS;
  snprintf(b, sizeof(b), T("Verlauf · %.1f h"), have * (BATT_INTERVAL_MS / 1000.0f) / 3600.0f);
  ui_label_update(a_scale, b);
}

lv_obj_t *page_akku_create() {
  lv_obj_t *s = ui_screen_create();
  a_ring = ui_ring(s, 396);

  lv_obj_t *t = ui_label(s, "Akku", &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  a_val = ui_label(s, "–", &font_sg_80, C_TEXT);
  lv_obj_align(a_val, LV_ALIGN_CENTER, 0, -62);
  a_unit = ui_label(s, "%", &font_sg_18, C_MUTED);
  lv_obj_align(a_unit, LV_ALIGN_CENTER, 0, -14);

  a_state = ui_label(s, "", &font_sg_24, C_TEXT);
  lv_obj_align(a_state, LV_ALIGN_CENTER, 0, 18);

  // Balkenverlauf
  lv_obj_t *chart = ui_box(s);
  lv_obj_set_size(chart, 240, 60);
  lv_obj_align(chart, LV_ALIGN_CENTER, 0, 76);
  lv_obj_set_style_border_width(chart, 0, 0);
  lv_obj_set_style_pad_all(chart, 0, 0);
  lv_obj_set_flex_flow(chart, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(chart, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(chart, 2, 0);
  for (int i = 0; i < BARS; i++) {
    a_bars[i] = ui_box(chart);
    lv_obj_set_width(a_bars[i], 7);
    lv_obj_set_height(a_bars[i], 2);
    lv_obj_set_style_radius(a_bars[i], 2, 0);
    lv_obj_set_style_bg_color(a_bars[i], C_TRACK, 0);
    lv_obj_set_style_bg_opa(a_bars[i], LV_OPA_COVER, 0);
  }

  a_scale = ui_label(s, "", &font_sg_14, C_FAINT);
  lv_obj_align(a_scale, LV_ALIGN_CENTER, 0, 118);
  a_info = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(a_info, LV_ALIGN_CENTER, 0, 142);

  a_test = ui_btn(s, "Entladetest", BTN_NORMAL);
  lv_obj_set_height(a_test, 42);
  lv_obj_align(a_test, LV_ALIGN_CENTER, 0, 174);
  lv_obj_add_event_cb(a_test, [](lv_event_t *e) {
    if (batt_test_running()) batt_test_stop();
    else batt_test_start();
    sound_play(SND_TICK);
  }, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, a_timer_cb, 1000);
  a_timer_cb(NULL);
  return s;
}
