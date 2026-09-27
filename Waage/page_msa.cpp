// ============================================================
//  Setup -> Waage -> Messmittelprüfung
//  Messmittelfähigkeit nach Verfahren 1: dasselbe Normal wird
//  25 oder 50 mal gemessen, jeweils auflegen, stabilisieren lassen,
//  abnehmen. Daraus berechnet die Waage:
//     x̄  Mittelwert der Einzelmessungen
//     Bi  systematische Abweichung (x̄ − Sollwert)
//     s   Standardabweichung der Einzelmessungen
//     Cg  = 0,2·T / (6·s)        (Streuung)
//     Cgk = (0,1·T − |Bi|) / (3·s)  (Lage)
//  Fähig ab 1,33 (Bosch-Formeln). T ist die Toleranz der Prüfaufgabe.
//  Ergebnis samt Einzelwerten landet in /Waage/Pruefung/<Datum>.txt
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "storage.h"
#include "data.h"
#include "sound.h"
#include "hal.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MSA_MAX 50
#define EMPTY_G 2.0f
#define STABLE_MS 800

static int m_nominal = 500;   // Sollgewicht des Normals in g
static int m_tol = 10;        // Toleranz T der Prüfaufgabe in g
static int m_runs = 25;       // Anzahl Messungen
static int m_step = 10;
static float m_val[MSA_MAX];
static int m_done = 0;

static lv_obj_t *page_run_create();
static lv_obj_t *page_result_create();

// ------------------------------------------------------------
//  Einrichten
// ------------------------------------------------------------
static lv_obj_t *su_nom, *su_tol, *su_runs[2], *su_steps[3];
static const int STEPS[3] = { 1, 10, 100 };
static const int RUNS[2] = { 25, 50 };

static void su_show() {
  char b[24];
  snprintf(b, sizeof(b), "%d g", m_nominal);
  lv_label_set_text(su_nom, b);
  snprintf(b, sizeof(b), "T = %d g", m_tol);
  lv_label_set_text(su_tol, b);
  for (int i = 0; i < 2; i++) {
    bool on = RUNS[i] == m_runs;
    lv_obj_set_style_bg_color(su_runs[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(su_runs[i], 0), on ? C_BG : C_TEXT2, 0);
  }
  for (int i = 0; i < 3; i++) {
    bool on = STEPS[i] == m_step;
    lv_obj_set_style_bg_color(su_steps[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(su_steps[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

// user_data: 0/1 = Sollgewicht −/+, 2/3 = Toleranz −/+
static void su_adj_cb(lv_event_t *e) {
  int v = (int)(intptr_t)lv_event_get_user_data(e);
  int d = (v & 1) ? m_step : -m_step;
  if (v < 2) {
    m_nominal += d;
    if (m_nominal < 1) m_nominal = 1;
    if (m_nominal > (int)WAAGE_MAX_G) m_nominal = (int)WAAGE_MAX_G;
  } else {
    m_tol += d;
    if (m_tol < 1) m_tol = 1;
  }
  su_show();
}

static void su_step_cb(lv_event_t *e) {
  m_step = (int)(intptr_t)lv_event_get_user_data(e);
  su_show();
}

static void su_runs_cb(lv_event_t *e) {
  m_runs = (int)(intptr_t)lv_event_get_user_data(e);
  su_show();
}

static void su_go_cb(lv_event_t *e) {
  m_done = 0;
  scale_tare();
  ui_switch_page(page_run_create());
}

static lv_obj_t *seg(lv_obj_t *row, const char *txt, lv_event_cb_t cb, int val) {
  lv_obj_t *b = ui_btn(row, txt, BTN_NORMAL);
  lv_obj_set_height(b, 40);
  lv_obj_set_style_pad_hor(b, 12, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)val);
  return b;
}

static lv_obj_t *row_at(lv_obj_t *s, lv_coord_t y) {
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
  return row;
}

lv_obj_t *page_msa_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Messmittelprüfung", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  su_nom = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(su_nom, LV_ALIGN_CENTER, 0, -96);
  lv_obj_t *m1 = ui_round_btn(s, "−", NULL);
  lv_obj_set_size(m1, 48, 48);
  lv_obj_align(m1, LV_ALIGN_CENTER, -118, -96);
  lv_obj_add_event_cb(m1, su_adj_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p1 = ui_round_btn(s, "+", NULL);
  lv_obj_set_size(p1, 48, 48);
  lv_obj_align(p1, LV_ALIGN_CENTER, 118, -96);
  lv_obj_add_event_cb(p1, su_adj_cb, LV_EVENT_CLICKED, (void *)1);

  su_tol = ui_label(s, "", &font_sg_24, C_TEXT);
  lv_obj_align(su_tol, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t *m2 = ui_round_btn(s, "−", NULL);
  lv_obj_set_size(m2, 44, 44);
  lv_obj_align(m2, LV_ALIGN_CENTER, -118, -40);
  lv_obj_add_event_cb(m2, su_adj_cb, LV_EVENT_CLICKED, (void *)2);
  lv_obj_t *p2 = ui_round_btn(s, "+", NULL);
  lv_obj_set_size(p2, 44, 44);
  lv_obj_align(p2, LV_ALIGN_CENTER, 118, -40);
  lv_obj_add_event_cb(p2, su_adj_cb, LV_EVENT_CLICKED, (void *)3);

  lv_obj_t *r1 = row_at(s, 6);
  for (int i = 0; i < 3; i++) {
    char b[6];
    snprintf(b, sizeof(b), "%d", STEPS[i]);
    su_steps[i] = seg(r1, b, su_step_cb, STEPS[i]);
  }

  lv_obj_t *r2 = row_at(s, 56);
  for (int i = 0; i < 2; i++) {
    char b[16];
    snprintf(b, sizeof(b), T("%d Messungen"), RUNS[i]);
    su_runs[i] = seg(r2, b, su_runs_cb, RUNS[i]);
  }

  lv_obj_t *go = ui_btn(s, "Start", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 34, 0);
  lv_obj_align(go, LV_ALIGN_CENTER, 0, 114);
  lv_obj_add_event_cb(go, su_go_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Normal auflegen, warten, abnehmen · Waage jetzt leer", &font_sg_14, C_FAINT);
  lv_obj_set_width(h, 300);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 152);

  su_show();
  return s;
}

// ------------------------------------------------------------
//  Messdurchlauf: Normal auflegen, ruhig werden lassen, abnehmen.
//  Der Wert wird automatisch übernommen, sobald die Waage 0,8 s ruhig
//  liegt - oder jederzeit von Hand über "Übernehmen". Das Gewicht steht
//  groß auf der Seite, damit man sieht, was die Waage gerade misst.
// ------------------------------------------------------------
typedef enum { M_PLACE, M_WAIT, M_REMOVE } mstate_t;
static mstate_t m_state;
static uint32_t m_stable_since;
static lv_obj_t *r_ring, *r_count, *r_hint, *r_weight, *r_take;

static float m_min_g() {  // ab hier gilt das Normal als aufgelegt
  float half = m_nominal * 0.3f;
  return half < 5.0f ? 5.0f : half;
}

static void r_take_value() {
  float g = scale_net();
  if (g < m_min_g() || m_done >= m_runs) return;
  m_val[m_done++] = g;
  sound_play(SND_TICK);
  m_state = M_REMOVE;
  m_stable_since = 0;
}

static void r_take_cb(lv_event_t *e) {
  r_take_value();
}

static void r_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  uint32_t now = hal_millis();
  char b[48];

  snprintf(b, sizeof(b), "%.1f", g);
  for (char *c = b; *c; c++)
    if (*c == '.') *c = ',';  // die große Schrift kennt nur Ziffern und Komma
  ui_label_update(r_weight, b);

  bool drauf = g >= m_min_g();
  bool fertig = m_done >= m_runs;

  switch (m_state) {
    case M_PLACE:
      ui_label_update(r_hint, "Normal auflegen");
      if (drauf) {
        m_state = M_WAIT;
        m_stable_since = 0;
      }
      break;
    case M_WAIT:
      if (!drauf) {
        m_state = M_PLACE;
        break;
      }
      if (!scale_stable()) {
        ui_label_update(r_hint, "stabilisieren …");
        m_stable_since = 0;
        break;
      }
      ui_label_update(r_hint, "ruhig – wird übernommen");
      if (m_stable_since == 0) m_stable_since = now;
      if (now - m_stable_since >= STABLE_MS) r_take_value();
      break;
    case M_REMOVE:
      ui_label_update(r_hint, fertig ? "fertig – abnehmen" : "Normal abnehmen");
      if (fabsf(g) < EMPTY_G) {
        if (fertig) {
          ui_switch_page(page_result_create());
          return;
        }
        m_state = M_PLACE;
      }
      break;
  }

  // Knopf nur aktiv, wenn wirklich etwas aufliegt
  if (drauf && m_state != M_REMOVE) lv_obj_clear_state(r_take, LV_STATE_DISABLED);
  else lv_obj_add_state(r_take, LV_STATE_DISABLED);

  snprintf(b, sizeof(b), "%d / %d", m_done, m_runs);
  ui_label_update(r_count, b);
  int v = m_runs > 0 ? m_done * 1000 / m_runs : 0;
  if (lv_arc_get_value(r_ring) != v) lv_arc_set_value(r_ring, v);
}

static lv_obj_t *page_run_create() {
  lv_obj_t *s = ui_screen_create();
  r_ring = ui_ring(s, 396);

  char b[40];
  snprintf(b, sizeof(b), T("Normal %d g · T %d g"), m_nominal, m_tol);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 46);

  r_count = ui_label(s, "0 / 25", &font_sg_18, C_ACCENT);
  lv_obj_align(r_count, LV_ALIGN_TOP_MID, 0, 74);

  r_weight = ui_label(s, "0,0", &font_sg_80, C_TEXT);
  lv_obj_align(r_weight, LV_ALIGN_CENTER, 0, -34);
  lv_obj_t *u = ui_label(s, "g", &font_sg_18, C_MUTED);
  lv_obj_align(u, LV_ALIGN_CENTER, 0, 6);

  r_hint = ui_label(s, "Normal auflegen", &font_sg_18, C_MUTED);
  lv_obj_align(r_hint, LV_ALIGN_CENTER, 0, 34);

  r_take = ui_btn(s, "Übernehmen", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(r_take, 26, 0);
  lv_obj_align(r_take, LV_ALIGN_CENTER, 0, 74);
  lv_obj_add_event_cb(r_take, r_take_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *b2 = ui_btn(s, "Abbrechen", BTN_NORMAL);
  lv_obj_align(b2, LV_ALIGN_CENTER, 0, 132);
  lv_obj_add_event_cb(b2, [](lv_event_t *e) { ui_switch_page(page_msa_create()); }, LV_EVENT_CLICKED, NULL);

  m_state = M_PLACE;
  m_stable_since = 0;
  ui_page_timer(s, r_timer_cb, 100);
  r_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Auswertung
// ------------------------------------------------------------
static lv_obj_t *page_result_create() {
  double sum = 0;
  for (int i = 0; i < m_done; i++) sum += m_val[i];
  double mean = m_done ? sum / m_done : 0;
  double var = 0;
  for (int i = 0; i < m_done; i++) var += (m_val[i] - mean) * (m_val[i] - mean);
  double sg = m_done > 1 ? sqrt(var / (m_done - 1)) : 0;
  double bias = mean - m_nominal;
  double tol = m_tol;  // Toleranz T (nicht T nennen, T() übersetzt)
  double cg = sg > 0 ? 0.2 * tol / (6 * sg) : 0;
  double cgk = sg > 0 ? (0.1 * tol - fabs(bias)) / (3 * sg) : 0;
  bool ok = cg >= 1.33 && cgk >= 1.33;

  // Ergebnis mit allen Einzelwerten auf die SD
  char day[11];
  log_today(day);
  int h, mi;
  hal_time(&h, &mi);
  char path[56], line[160];
  snprintf(path, sizeof(path), "/Waage/Pruefung/%s.txt", day[0] ? day : "unbekannt");
  snprintf(line, sizeof(line),
           "%02d:%02d;Verfahren 1;n=%d;Soll=%d g;T=%d g;Mittel=%.3f;Bi=%.3f;s=%.3f;Cg=%.2f;Cgk=%.2f;%s", h, mi,
           m_done, m_nominal, m_tol, mean, bias, sg, cg, cgk, ok ? "faehig" : "nicht faehig");
  storage_append(path, line);
  line[0] = 0;
  for (int i = 0; i < m_done; i++) {  // Einzelwerte in Zeilen zu je zehn
    char one[12];
    snprintf(one, sizeof(one), "%s%.2f", i % 10 ? ";" : "", m_val[i]);
    strncat(line, one, sizeof(line) - strlen(line) - 1);
    if (i % 10 == 9 || i == m_done - 1) {
      storage_append(path, line);
      line[0] = 0;
    }
  }
  sound_play(ok ? SND_DONE : SND_WARN);

  lv_obj_t *s = ui_screen_create();
  lv_obj_t *ring = ui_ring(s, 396);
  lv_arc_set_value(ring, 1000);
  lv_obj_set_style_arc_color(ring, ok ? C_ACCENT : C_WARN, LV_PART_INDICATOR);

  lv_obj_t *t = ui_label(s, "Messmittelfähigkeit", &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  char b[48];
  snprintf(b, sizeof(b), T("Cg %.2f"), cg);
  lv_obj_t *l1 = ui_label(s, b, &font_sg_34, cg >= 1.33 ? C_ACCENT : C_WARN);
  lv_obj_align(l1, LV_ALIGN_CENTER, 0, -74);
  snprintf(b, sizeof(b), T("Cgk %.2f"), cgk);
  lv_obj_t *l2 = ui_label(s, b, &font_sg_34, cgk >= 1.33 ? C_ACCENT : C_WARN);
  lv_obj_align(l2, LV_ALIGN_CENTER, 0, -28);

  lv_obj_t *v = ui_label(s, ok ? "fähig (≥ 1,33)" : "nicht fähig", &font_sg_18, ok ? C_ACCENT : C_WARN);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 14);

  snprintf(b, sizeof(b), T("Mittel %.2f g · Bi %+.2f g"), mean, bias);
  lv_obj_t *l3 = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(l3, LV_ALIGN_CENTER, 0, 48);
  snprintf(b, sizeof(b), "s %.3f g · n %d · T %d g", sg, m_done, m_tol);
  lv_obj_t *l4 = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(l4, LV_ALIGN_CENTER, 0, 70);

  // Kleinste prüfbare Toleranz: T_min = 40·s + 10·|Bi|  (aus Cgk = 1,33)
  snprintf(b, sizeof(b), T("kleinste prüfbare Toleranz %.1f g"), 40 * sg + 10 * fabs(bias));
  lv_obj_t *l5 = ui_label(s, b, &font_sg_14, C_FAINT);
  lv_obj_align(l5, LV_ALIGN_CENTER, 0, 96);

  lv_obj_t *b2 = ui_btn(s, "Fertig", BTN_PRIMARY);
  lv_obj_align(b2, LV_ALIGN_CENTER, 0, 138);
  lv_obj_add_event_cb(b2, [](lv_event_t *e) { ui_go_home(); }, LV_EVENT_CLICKED, NULL);
  return s;
}
