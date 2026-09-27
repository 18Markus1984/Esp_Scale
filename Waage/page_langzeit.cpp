// ============================================================
//  Langzeitmessung (Werkstatt): Gewicht über Stunden/Tage auf die
//  SD-Karte schreiben, z. B. Filament trocknen, Verdunstung, Teiggare.
//  Die Messung läuft im Hintergrund weiter (tools.cpp), auch wenn die
//  Seite geschlossen wird; Auto-Aus ist solange pausiert.
//  Kurve und Datei gibt es auch im Browser (Weboberfläche -> Messungen).
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "sound.h"
#include "tools.h"
#include <stdio.h>
#include <math.h>

#define CH_W 250
#define CH_H 96

static int l_iv = 60;  // gewähltes Intervall in s
static const int IVS[4] = { 10, 60, 300, 900 };
static const char *const IV_T[4] = { "10 s", "1 min", "5 min", "15 min" };

static lv_obj_t *l_weight, *l_info, *l_trend, *l_line, *l_frame, *l_minmax;
static lv_obj_t *l_iv_btns[4], *l_setup, *l_run;
static lv_point_t l_pts[LT_HIST];

static void l_mark() {
  for (int i = 0; i < 4; i++) {
    bool on = IVS[i] == l_iv;
    lv_obj_set_style_bg_color(l_iv_btns[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(l_iv_btns[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

static void l_timer_cb(lv_timer_t *t) {
  char b[64], w[16];
  ui_fmt_weight(w, sizeof(w), scale_net());
  snprintf(b, sizeof(b), "%s g", w);
  ui_label_update(l_weight, b);

  bool run = lt_running();
  if (run) {
    lv_obj_add_flag(l_setup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(l_run, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(l_setup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(l_run, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  char h[16];
  ui_fmt_1dec(h, sizeof(h), lt_hours());
  int iv = lt_interval();
  if (iv >= 60) snprintf(b, sizeof(b), T("%s h · %d Werte · alle %d min"), h, lt_samples(), iv / 60);
  else snprintf(b, sizeof(b), T("%s h · %d Werte · alle %d s"), h, lt_samples(), iv);
  ui_label_update(l_info, b);

  float tr = lt_trend();
  ui_fmt_1dec(w, sizeof(w), tr);
  if (lt_samples() < 3) snprintf(b, sizeof(b), "%s", T("Trend folgt …"));
  else snprintf(b, sizeof(b), T("Trend %s%s g/h"), tr > 0 ? "+" : "", w);
  ui_label_update(l_trend, b);

  // Kurve: letzte Werte auf die Fläche skalieren
  static float v[LT_HIST];
  int n = lt_hist(v, LT_HIST);
  if (n < 2) {
    lv_obj_add_flag(l_line, LV_OBJ_FLAG_HIDDEN);
    ui_label_update(l_minmax, "");
    return;
  }
  float lo = v[0], hi = v[0];
  for (int i = 1; i < n; i++) {
    if (v[i] < lo) lo = v[i];
    if (v[i] > hi) hi = v[i];
  }
  float span = hi - lo < 0.5f ? 0.5f : hi - lo;  // flache Kurven nicht aufblasen
  float mid = (hi + lo) / 2;
  for (int i = 0; i < n; i++) {
    l_pts[i].x = (lv_coord_t)(i * (CH_W - 1) / (n - 1));
    l_pts[i].y = (lv_coord_t)(CH_H / 2 - (v[i] - mid) / span * (CH_H - 8));
  }
  lv_line_set_points(l_line, l_pts, n);
  lv_obj_clear_flag(l_line, LV_OBJ_FLAG_HIDDEN);
  char a[16], c[16];
  ui_fmt_1dec(a, sizeof(a), lo);
  ui_fmt_1dec(c, sizeof(c), hi);
  snprintf(b, sizeof(b), T("min %s g · max %s g"), a, c);
  ui_label_update(l_minmax, b);
}

static void l_iv_cb(lv_event_t *e) {
  l_iv = (int)(intptr_t)lv_event_get_user_data(e);
  sound_play(SND_CLICK);
  l_mark();
}

lv_obj_t *page_langzeit_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Langzeitmessung", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);

  l_weight = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(l_weight, LV_ALIGN_TOP_MID, 0, 80);

  // --- nicht laufend: Intervall wählen, Start ---
  l_setup = ui_box(s);
  lv_obj_set_size(l_setup, 380, 250);
  lv_obj_align(l_setup, LV_ALIGN_CENTER, 0, 50);
  lv_obj_t *il = ui_label(l_setup, "Messen alle", &font_sg_14, C_MUTED);
  lv_obj_align(il, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *row = ui_box(l_setup);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 24);
  for (int i = 0; i < 4; i++) {
    l_iv_btns[i] = ui_btn(row, IV_T[i], BTN_NORMAL);
    lv_obj_set_height(l_iv_btns[i], 40);
    lv_obj_set_style_pad_hor(l_iv_btns[i], 12, 0);
    lv_obj_add_event_cb(l_iv_btns[i], l_iv_cb, LV_EVENT_CLICKED, (void *)(intptr_t)IVS[i]);
  }
  lv_obj_t *hint = ui_label(l_setup, "Schreibt nach /Waage/Messung.\nAuto-Aus ist währenddessen pausiert.",
                            &font_sg_14, C_FAINT);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 80);
  lv_obj_t *start = ui_btn(l_setup, "Start", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(start, 30, 0);
  lv_obj_align(start, LV_ALIGN_TOP_MID, 0, 132);
  lv_obj_add_event_cb(start, [](lv_event_t *e) {
    if (lt_start(l_iv)) sound_play(SND_SAVE);
    else sound_play(SND_WARN);  // keine SD-Karte
  }, LV_EVENT_CLICKED, NULL);

  // --- laufend: Kurve, Kennzahlen, Stopp ---
  l_run = ui_box(s);
  lv_obj_set_size(l_run, 380, 270);
  lv_obj_align(l_run, LV_ALIGN_CENTER, 0, 56);
  l_frame = ui_box(l_run);
  lv_obj_set_size(l_frame, CH_W + 4, CH_H + 4);
  lv_obj_set_style_border_width(l_frame, 1, 0);
  lv_obj_set_style_border_color(l_frame, C_TRACK, 0);
  lv_obj_set_style_radius(l_frame, 8, 0);
  lv_obj_align(l_frame, LV_ALIGN_TOP_MID, 0, 0);
  l_line = lv_line_create(l_frame);
  lv_obj_set_pos(l_line, 1, 1);
  lv_obj_set_style_line_width(l_line, 3, 0);
  lv_obj_set_style_line_color(l_line, C_ACCENT, 0);
  lv_obj_set_style_line_rounded(l_line, true, 0);
  l_minmax = ui_label(l_run, "", &font_sg_14, C_FAINT);
  lv_obj_align(l_minmax, LV_ALIGN_TOP_MID, 0, CH_H + 8);
  l_trend = ui_label(l_run, "", &font_sg_18, C_ACCENT);
  lv_obj_align(l_trend, LV_ALIGN_TOP_MID, 0, CH_H + 30);
  l_info = ui_label(l_run, "", &font_sg_14, C_MUTED);
  lv_obj_align(l_info, LV_ALIGN_TOP_MID, 0, CH_H + 56);
  lv_obj_t *stop = ui_btn(l_run, "Stopp", BTN_WARN);
  lv_obj_align(stop, LV_ALIGN_TOP_MID, 0, CH_H + 82);
  lv_obj_add_event_cb(stop, [](lv_event_t *e) {
    lt_stop();
    sound_play(SND_DONE);
  }, LV_EVENT_CLICKED, NULL);

  l_mark();
  ui_page_timer(s, l_timer_cb, 1000);
  l_timer_cb(NULL);
  return s;
}
