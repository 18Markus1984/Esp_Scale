// ============================================================
//  System -> Kalibrierung (siehe Design-Sheet)
//   1/3  Waage leeren          -> Nullpunkt messen
//   2/3  Referenz auflegen     -> Gewicht einstellen, Faktor berechnen
//   3/3  Gespeichert           -> Kontrollanzeige mit neuem Faktor
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "config.h"
#include "sound.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

static int ref_g = 500;    // bleibt für die nächste Kalibrierung erhalten
static int ref_step = 100;
static long k_zero = 0;    // Rohwert der leeren Waage aus Schritt 1
static long k_raw[CAL_MAX];   // gesammelte Referenzpunkte
static float k_g[CAL_MAX];
static int k_n = 0;

static lv_obj_t *page_step2_create();
static lv_obj_t *page_step3_create();

static lv_obj_t *step_title(lv_obj_t *s, int step) {
  char b[32];
  snprintf(b, sizeof(b), T("Kalibrierung · %d / 3"), step);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);
  return t;
}

// Live-Gewicht und Stabilität für alle drei Schritte
static lv_obj_t *k_live, *k_state, *k_toast;

static void live_cb(lv_timer_t *t) {
  char w[16], b[24];
  ui_fmt_weight(w, sizeof(w), scale_gross());
  snprintf(b, sizeof(b), "%s g", w);
  ui_label_update(k_live, b);
  if (k_state) {
    bool st = scale_stable();
    ui_chip_set(k_state, st ? "stabil" : "misst …", st ? C_ACCENT : C_MUTED, false);
  }
}

// ------------------------------------------------------------
//  Schritt 1: Waage leeren
// ------------------------------------------------------------
static void s1_next_cb(lv_event_t *e) {
  if (!scale_stable()) {
    ui_toast_show(k_toast, "Noch nicht stabil", C_WARN);
    return;
  }
  k_zero = scale_raw();
  k_n = 0;
  ui_switch_page(page_step2_create());
}

lv_obj_t *page_kalib_create() {
  lv_obj_t *s = ui_screen_create();
  step_title(s, 1);

  lv_obj_t *h = ui_label(s, "Waage leeren", &font_sg_24, C_TEXT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -70);

  k_live = ui_label(s, "", &font_sg_34, C_MUTED);
  lv_obj_align(k_live, LV_ALIGN_CENTER, 0, -20);
  k_state = ui_chip(s, "misst …", C_MUTED);
  lv_obj_align(k_state, LV_ALIGN_CENTER, 0, 22);

  lv_obj_t *info = ui_label(s, "Nichts auflegen, dann weiter\n„Prüfen“ = Messmittelfähigkeit", &font_sg_14, C_MUTED);
  lv_obj_align(info, LV_ALIGN_CENTER, 0, 56);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 104);
  lv_obj_t *msa = ui_btn(row, "Prüfen ›", BTN_NORMAL);
  lv_obj_add_event_cb(msa, [](lv_event_t *e) { ui_switch_page(page_msa_create()); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *b = ui_btn(row, "Weiter", BTN_PRIMARY);
  lv_obj_add_event_cb(b, s1_next_cb, LV_EVENT_CLICKED, NULL);

  k_toast = ui_toast_create(s);
  lv_obj_align(k_toast, LV_ALIGN_CENTER, 0, 152);

  ui_page_timer(s, live_cb, 100);
  live_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Schritt 2: Referenz auflegen
// ------------------------------------------------------------
static lv_obj_t *s2_value;
static lv_obj_t *s2_steps[3];
static const int STEPS[3] = { 1, 10, 100 };

static void s2_show() {
  char b[16];
  snprintf(b, sizeof(b), "%d g", ref_g);
  lv_label_set_text(s2_value, b);
  for (int i = 0; i < 3; i++) {
    bool on = STEPS[i] == ref_step;
    lv_obj_set_style_bg_color(s2_steps[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s2_steps[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

static void s2_minus_cb(lv_event_t *e) {
  ref_g -= ref_step;
  if (ref_g < 1) ref_g = 1;
  s2_show();
}

static void s2_plus_cb(lv_event_t *e) {
  ref_g += ref_step;
  if (ref_g > (int)WAAGE_MAX_G) ref_g = (int)WAAGE_MAX_G;
  s2_show();
}

static void s2_step_cb(lv_event_t *e) {
  ref_step = (int)(intptr_t)lv_event_get_user_data(e);
  s2_show();
}

static void s2_calib_cb(lv_event_t *e) {
  if (!scale_stable()) {
    ui_toast_show(k_toast, "Noch nicht stabil", C_WARN);
    return;
  }
  if (k_n > 0 && ref_g <= (int)k_g[k_n - 1]) {
    ui_toast_show(k_toast, "Nächster Punkt muss schwerer sein", C_WARN);
    return;
  }
  k_raw[k_n] = scale_raw();
  k_g[k_n] = (float)ref_g;
  k_n++;
  if (!scale_calibrate_points(k_zero, k_raw, k_g, k_n)) {
    sound_play(SND_WARN);
    k_n--;
    ui_toast_show(k_toast, "Kein Gewicht erkannt", C_WARN);
    return;
  }
  sound_play(SND_DONE);
  ui_pots_changed();  // alte Tara/Topf-Zustände passen nicht mehr
  ui_switch_page(page_step3_create());
}

static lv_obj_t *page_step2_create() {
  lv_obj_t *s = ui_screen_create();
  step_title(s, 2);

  lv_obj_t *h = ui_label(s, "Referenz auflegen", &font_sg_24, C_TEXT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -100);

  s2_value = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(s2_value, LV_ALIGN_CENTER, 0, -45);
  lv_obj_t *m = ui_round_btn(s, "−", s2_minus_cb);
  lv_obj_align(m, LV_ALIGN_CENTER, -120, -45);
  lv_obj_t *p = ui_round_btn(s, "+", s2_plus_cb);
  lv_obj_align(p, LV_ALIGN_CENTER, 120, -45);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 14);
  for (int i = 0; i < 3; i++) {
    char b[8];
    snprintf(b, sizeof(b), "%d", STEPS[i]);
    s2_steps[i] = ui_btn(row, b, BTN_NORMAL);
    lv_obj_set_height(s2_steps[i], 44);
    lv_obj_set_style_pad_hor(s2_steps[i], 16, 0);
    lv_obj_add_event_cb(s2_steps[i], s2_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)STEPS[i]);
  }

  // kleine Live-Anzeige mit dem alten Faktor, damit man sieht, dass etwas aufliegt
  k_live = ui_label(s, "", &font_sg_14, C_FAINT);
  lv_obj_align(k_live, LV_ALIGN_CENTER, 0, 56);
  k_state = NULL;

  lv_obj_t *b = ui_btn(s, "Kalibrieren", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 104);
  lv_obj_add_event_cb(b, s2_calib_cb, LV_EVENT_CLICKED, NULL);

  k_toast = ui_toast_create(s);
  lv_obj_align(k_toast, LV_ALIGN_CENTER, 0, 152);

  ui_page_timer(s, live_cb, 100);
  s2_show();
  live_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Schritt 3: Gespeichert + Kontrolle
// ------------------------------------------------------------
static void s3_done_cb(lv_event_t *e) {
  ui_go_home();
}

static lv_obj_t *page_step3_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *ring = ui_ring(s, 396);
  lv_arc_set_value(ring, 1000);
  step_title(s, 3);

  lv_obj_t *h = ui_label(s, "Gespeichert", &font_sg_24, C_ACCENT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -76);

  k_live = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(k_live, LV_ALIGN_CENTER, 0, -28);
  k_state = NULL;

  char f[16], b[40];
  snprintf(f, sizeof(f), "%.2f", scale_factor());
  for (char *c = f; *c; c++) if (*c == '.') *c = ',';
  snprintf(b, sizeof(b), T("Faktor %s"), f);
  lv_obj_t *fl = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(fl, LV_ALIGN_CENTER, 0, 14);

  char pts[64] = "";
  for (int i = 0; i < k_n; i++) {
    char one[16];
    snprintf(one, sizeof(one), "%s%d g", i ? " · " : "", (int)lroundf(k_g[i]));
    strncat(pts, one, sizeof(pts) - strlen(pts) - 1);
  }
  lv_obj_t *pl = ui_label(s, pts, &font_sg_14, C_ACCENT);
  lv_obj_align(pl, LV_ALIGN_CENTER, 0, 36);

  lv_obj_t *hint = ui_label(s, k_n < CAL_MAX ? "„Punkt +“ für einen weiteren\nStützpunkt (genauer über den Bereich)"
                                             : "Drei Punkte gespeichert", &font_sg_14, C_FAINT);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 66);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 110);
  if (k_n < CAL_MAX) {  // mit zwei oder drei Punkten wird es über den Bereich genauer
    lv_obj_t *more = ui_btn(row, "Punkt +", BTN_NORMAL);
    lv_obj_add_event_cb(more, [](lv_event_t *e) {
      ref_g = (int)(k_g[k_n - 1] * 2);
      if (ref_g > (int)WAAGE_MAX_G) ref_g = (int)WAAGE_MAX_G;
      ui_switch_page(page_step2_create());
    }, LV_EVENT_CLICKED, NULL);
  }
  lv_obj_t *b2 = ui_btn(row, "Fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(b2, s3_done_cb, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, live_cb, 100);
  live_cb(NULL);
  return s;
}
