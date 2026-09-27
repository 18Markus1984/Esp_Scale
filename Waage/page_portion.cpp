// ============================================================
//  Modus "Portionieren" (Küche): Teig o. Ä. in n gleiche Teile
//
//  1. Ganzes auflegen (vorher ggf. Schüssel tarieren), Stückzahl wählen
//  2. Waage leeren -> Tara läuft von selbst
//  3. Stück für Stück auflegen: Ring und "+4 g" / "passt" zeigen die
//     Abweichung, der Parkpiepser hilft beim Nachschneiden. Beim Abnehmen
//     wird der letzte ruhige Wert übernommen. Das Soll jedes Stücks wird
//     aus dem, was noch übrig ist, neu berechnet, damit sich Fehler nicht
//     aufsummieren.
//  4. Ergebnis mit Durchschnitt, kleinstem/größtem Stück und Streuung
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "sound.h"
#include "data.h"
#include <stdio.h>
#include <math.h>

#define PORTION_MAX 60
#define EMPTY_G 3.0f       // darunter gilt die Waage als leer
#define ACCEPT_MS 600      // so lange muss ein Stück ruhig liegen

static int p_count = 12;              // bleibt bis zum nächsten Mal erhalten
static float p_total = 0;             // Gewicht des Ganzen
static float p_pieces[PORTION_MAX];   // übernommene Stücke
static int p_done = 0;

static lv_obj_t *page_portion_run_create();
static lv_obj_t *page_portion_result_create();

// Soll des nächsten Stücks: Rest gleichmäßig auf die restlichen Stücke
static float p_target() {
  float sum = 0;
  for (int i = 0; i < p_done; i++) sum += p_pieces[i];
  int left = p_count - p_done;
  return left > 0 ? (p_total - sum) / left : 0;
}

static float p_tol(float target) {
  float t = target * 0.02f;  // 2 %, mindestens 1 g
  return t < 1.0f ? 1.0f : t;
}

// "+4,2 g" / "−12 g"
static void fmt_dev(char *b, int len, float d) {
  char w[16];
  if (fabsf(d) >= 10.0f) snprintf(w, sizeof(w), "%d", (int)lroundf(fabsf(d)));
  else ui_fmt_1dec(w, sizeof(w), fabsf(d));
  snprintf(b, len, "%s%s g", d < 0 ? "−" : "+", w);
}

// ------------------------------------------------------------
//  Schritt 1: Ganzes wiegen, Stückzahl wählen
// ------------------------------------------------------------
static lv_obj_t *ps_weight, *ps_each, *ps_count, *ps_next;

static void ps_show_count() {
  char b[24];
  snprintf(b, sizeof(b), T("%d Stück"), p_count);
  lv_label_set_text(ps_count, b);
}

static void ps_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  char b[32], w[16];
  ui_fmt_weight(w, sizeof(w), g);
  snprintf(b, sizeof(b), "%s g", w);
  ui_label_update(ps_weight, b);
  if (g > EMPTY_G) {
    ui_fmt_1dec(w, sizeof(w), g / p_count);
    snprintf(b, sizeof(b), T("je Stück %s g"), w);
  } else {
    snprintf(b, sizeof(b), "%s", T("Ganzes auflegen"));
  }
  ui_label_update(ps_each, b);
  bool ok = g > EMPTY_G && scale_stable() && !scale_overload();
  if (ok) lv_obj_clear_state(ps_next, LV_STATE_DISABLED);
  else lv_obj_add_state(ps_next, LV_STATE_DISABLED);
}

static void ps_adj_cb(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  p_count += d;
  if (p_count < 2) p_count = 2;
  if (p_count > PORTION_MAX) p_count = PORTION_MAX;
  ps_show_count();
  sound_play(SND_CLICK);
}

static void ps_next_cb(lv_event_t *e) {
  if (!scale_stable() || scale_net() <= EMPTY_G) {
    sound_play(SND_WARN);
    return;
  }
  p_total = scale_net();
  p_done = 0;
  sound_play(SND_SAVE);
  ui_switch_page(page_portion_run_create());
}

lv_obj_t *page_portion_create() {
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *t = ui_label(s, "Portionieren", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);

  ps_weight = ui_label(s, "0,0 g", &font_sg_34, C_TEXT);
  lv_obj_align(ps_weight, LV_ALIGN_CENTER, 0, -78);

  ps_each = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_align(ps_each, LV_ALIGN_CENTER, 0, -36);

  lv_obj_t *minus = ui_round_btn(s, "−", NULL);
  lv_obj_align(minus, LV_ALIGN_CENTER, -120, 26);
  lv_obj_add_event_cb(minus, ps_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
  lv_obj_t *plus = ui_round_btn(s, "+", NULL);
  lv_obj_align(plus, LV_ALIGN_CENTER, 120, 26);
  lv_obj_add_event_cb(plus, ps_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
  ps_count = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(ps_count, LV_ALIGN_CENTER, 0, 26);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 112);
  lv_obj_t *tara = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_add_event_cb(tara, [](lv_event_t *e) {
    scale_tare();
    sound_play(SND_TARA);
  }, LV_EVENT_CLICKED, NULL);
  ps_next = ui_btn(row, "Weiter", BTN_PRIMARY);
  lv_obj_add_event_cb(ps_next, ps_next_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Schüssel vorher tarieren", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 162);

  ps_show_count();
  ui_page_timer(s, ps_timer_cb, 100);
  ps_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Schritt 2: Stücke einzeln wiegen
// ------------------------------------------------------------
static lv_obj_t *pr_ring, *pr_title, *pr_weight, *pr_dev, *pr_sub;
static bool pr_ready;          // Waage wurde nach dem Ganzen geleert und tariert
static bool pr_on;             // liegt gerade ein Stück?
static float pr_last;          // letzter ruhiger Wert des aufgelegten Stücks
static uint32_t pr_stable_since;
static int pr_prev_state;

static void pr_update_texts() {
  char b[48], w[16], r[16];
  snprintf(b, sizeof(b), T("Stück %d / %d"), p_done + 1, p_count);
  ui_label_update(pr_title, b);
  float target = p_target();
  float sum = 0;
  for (int i = 0; i < p_done; i++) sum += p_pieces[i];
  ui_fmt_1dec(w, sizeof(w), target);
  ui_fmt_1dec(r, sizeof(r), p_total - sum);
  if (p_done + 1 == p_count) snprintf(b, sizeof(b), T("Ziel %s g · letztes Stück"), w);
  else snprintf(b, sizeof(b), T("Ziel %s g · Rest %s g"), w, r);
  ui_label_update(pr_sub, b);
}

static void pr_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  uint32_t now = lv_tick_get();

  // Erst leeren: nach dem Ganzen die Waage frei machen, dann automatisch tarieren
  if (!pr_ready) {
    ui_label_update(pr_weight, "-");
    ui_label_update(pr_dev, T("Waage leeren"));
    if (fabsf(scale_gross()) < 5.0f && scale_stable()) {
      scale_tare();
      sound_play(SND_TARA);
      pr_ready = true;
      pr_update_texts();
    }
    return;
  }

  float target = p_target();
  float tol = p_tol(target);
  char b[32], w[16];
  ui_fmt_weight(w, sizeof(w), g);
  ui_label_update(pr_weight, w);

  int v = target > 0 ? (int)(g / target * 1000.0f) : 0;
  if (v < 0) v = 0;
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(pr_ring) != v) lv_arc_set_value(pr_ring, v);

  if (g < EMPTY_G) {
    // Waage leer: war vorher ein Stück drauf, jetzt übernehmen
    if (pr_on && pr_last > EMPTY_G) {
      p_pieces[p_done++] = pr_last;
      sound_play(SND_SAVE);
      pr_on = false;
      pr_last = 0;
      sound_parking_reset();
      if (p_done >= p_count) {
        ui_switch_page(page_portion_result_create());
        return;
      }
      pr_update_texts();
    }
    ui_label_update(pr_dev, T("Stück auflegen"));
    lv_obj_set_style_text_color(pr_dev, C_MUTED, 0);
    pr_prev_state = -1;
    pr_stable_since = 0;
    return;
  }

  pr_on = true;
  float d = g - target;
  int state = d < -tol ? 0 : (d <= tol ? 1 : 2);  // zu leicht / passt / zu schwer
  if (state == 1) snprintf(b, sizeof(b), "%s", T("passt"));
  else fmt_dev(b, sizeof(b), d);
  ui_label_update(pr_dev, b);
  if (state != pr_prev_state) {
    pr_prev_state = state;
    lv_color_t c = state == 1 ? C_ACCENT : (state == 2 ? C_DANGER : C_WARN);
    lv_obj_set_style_text_color(pr_dev, c, 0);
    lv_obj_set_style_arc_color(pr_ring, state == 2 ? C_DANGER : C_ACCENT, LV_PART_INDICATOR);
  }
  sound_parking(g, target, tol);

  // letzter ruhiger Wert zählt (nach dem Nachschneiden)
  if (scale_stable()) {
    if (!pr_stable_since) pr_stable_since = now;
    if (now - pr_stable_since >= ACCEPT_MS) pr_last = g;
  } else {
    pr_stable_since = 0;
  }
}

static void pr_finish_cb(lv_event_t *e) {
  if (p_done == 0) {
    ui_switch_page(page_portion_create());
    return;
  }
  ui_switch_page(page_portion_result_create());
}

static lv_obj_t *page_portion_run_create() {
  lv_obj_t *s = ui_screen_create();
  pr_ring = ui_ring(s, 396);

  pr_title = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(pr_title, LV_ALIGN_TOP_MID, 0, 56);

  pr_weight = ui_label(s, "-", &font_sg_80, C_TEXT);
  lv_obj_align(pr_weight, LV_ALIGN_CENTER, 0, -34);

  pr_dev = ui_label(s, "", &font_sg_34, C_MUTED);
  lv_obj_align(pr_dev, LV_ALIGN_CENTER, 0, 36);

  pr_sub = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(pr_sub, LV_ALIGN_CENTER, 0, 76);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 124);
  lv_obj_t *tara = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_add_event_cb(tara, [](lv_event_t *e) {
    scale_tare();
    sound_play(SND_TARA);
    if (!pr_ready) {  // Waage ist leer, auch wenn der Nullpunkt nicht stimmt
      pr_ready = true;
      pr_update_texts();
    }
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *fin = ui_btn(row, "Fertig", BTN_NORMAL);
  lv_obj_add_event_cb(fin, pr_finish_cb, LV_EVENT_CLICKED, NULL);

  pr_ready = false;
  pr_on = false;
  pr_last = 0;
  pr_stable_since = 0;
  pr_prev_state = -1;
  sound_parking_reset();
  pr_update_texts();
  ui_page_timer(s, pr_timer_cb, 100);
  pr_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Schritt 3: Ergebnis
// ------------------------------------------------------------
static lv_obj_t *page_portion_result_create() {
  lv_obj_t *s = ui_screen_create();
  sound_parking_reset();
  sound_play(SND_DONE);

  float sum = 0, lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < p_done; i++) {
    sum += p_pieces[i];
    if (p_pieces[i] < lo) lo = p_pieces[i];
    if (p_pieces[i] > hi) hi = p_pieces[i];
  }
  float mean = p_done ? sum / p_done : 0;
  float var = 0;
  for (int i = 0; i < p_done; i++) var += (p_pieces[i] - mean) * (p_pieces[i] - mean);
  float sd = p_done > 1 ? sqrtf(var / (p_done - 1)) : 0;

  char b[64], a[16], c[16];
  lv_obj_t *t = ui_label(s, "Fertig portioniert", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);

  snprintf(b, sizeof(b), T("%d Stück"), p_done);
  lv_obj_t *n = ui_label(s, b, &font_sg_34, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -76);

  ui_fmt_1dec(a, sizeof(a), mean);
  snprintf(b, sizeof(b), T("Ø %s g"), a);
  lv_obj_t *m = ui_label(s, b, &font_sg_34, C_ACCENT);
  lv_obj_align(m, LV_ALIGN_CENTER, 0, -30);

  ui_fmt_1dec(a, sizeof(a), lo);
  ui_fmt_1dec(c, sizeof(c), hi);
  snprintf(b, sizeof(b), T("kleinstes %s g · größtes %s g"), a, c);
  lv_obj_t *r = ui_label(s, b, &font_sg_14, C_TEXT2);
  lv_obj_align(r, LV_ALIGN_CENTER, 0, 14);

  ui_fmt_1dec(a, sizeof(a), sd);
  snprintf(b, sizeof(b), T("Streuung s = %s g"), a);
  lv_obj_t *d = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(d, LV_ALIGN_CENTER, 0, 38);

  // ins Protokoll: Summe aller Stücke, Notiz mit Anzahl und Durchschnitt
  char note[40];
  ui_fmt_1dec(a, sizeof(a), mean);
  snprintf(note, sizeof(note), T("Portionen: %d × %s g"), p_done, a);
  log_add(sum, note);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 104);
  lv_obj_t *again = ui_btn(row, "Nochmal", BTN_PRIMARY);
  lv_obj_add_event_cb(again, [](lv_event_t *e) { ui_switch_page(page_portion_create()); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *home = ui_btn(row, "Zu Wiegen", BTN_NORMAL);
  lv_obj_add_event_cb(home, [](lv_event_t *e) { ui_go_home(); }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Im Protokoll gespeichert", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 158);
  return s;
}
