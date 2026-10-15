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
//  Ergebnis samt Einzelwerten landet als CSV in /Waage/Pruefung/
//  (msa_store.cpp); die Weboberfläche zeichnet daraus die Diagramme.
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
#include "msa_store.h"
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
static msa_stats_t m_res;       // Auswertung der letzten Prüfung
static bool m_saved = false;     // schon gespeichert (Ergebnisseite wird beim Zurückkommen neu gebaut)
static char m_file[40] = "";     // Dateiname auf der SD-Karte

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
  m_saved = false;
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

static lv_obj_t *page_chart_create();

// Dezimalpunkt -> Komma (wie das Gewicht auf der Wiegeseite)
static void comma(char *b) {
  for (char *c = b; *c; c++)
    if (*c == '.') *c = ',';
}

// Datum "2026-10-15 09:05" -> "15.10.2026"
static void de_date(char *b, int len, const char *iso) {
  if (strlen(iso) >= 10) snprintf(b, len, "%.2s.%.2s.%.4s", iso + 8, iso + 5, iso);
  else snprintf(b, len, "%s", T("unbekannt"));
}

static lv_obj_t *page_result_create() {
  if (!m_saved) {
    msa_calc(m_val, m_done, (float)m_nominal, (float)m_tol, &m_res);
    // Ergebnis mit allen Einzelwerten als CSV auf die SD (Diagramme im Browser)
    if (!msa_save((float)m_nominal, (float)m_tol, m_val, m_done, "Waage", m_file, sizeof(m_file))) m_file[0] = 0;
    m_saved = true;
    sound_play(m_res.ok ? SND_DONE : SND_WARN);
  }
  double mean = m_res.mean, bias = m_res.bias, sg = m_res.s, cg = m_res.cg, cgk = m_res.cgk;
  bool ok = m_res.ok;

  lv_obj_t *s = ui_screen_create();
  lv_obj_t *ring = ui_ring(s, 396);
  lv_arc_set_value(ring, 1000);
  lv_obj_set_style_arc_color(ring, ok ? C_ACCENT : C_WARN, LV_PART_INDICATOR);

  lv_obj_t *t = ui_label(s, "Messmittelfähigkeit", &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  char b[48];
  snprintf(b, sizeof(b), T("Cg %.2f"), cg);
  comma(b);
  lv_obj_t *l1 = ui_label(s, b, &font_sg_34, cg >= 1.33 ? C_ACCENT : C_WARN);
  lv_obj_align(l1, LV_ALIGN_CENTER, 0, -82);
  snprintf(b, sizeof(b), T("Cgk %.2f"), cgk);
  comma(b);
  lv_obj_t *l2 = ui_label(s, b, &font_sg_34, cgk >= 1.33 ? C_ACCENT : C_WARN);
  lv_obj_align(l2, LV_ALIGN_CENTER, 0, -36);

  lv_obj_t *v = ui_label(s, ok ? "fähig (ab 1,33)" : "nicht fähig", &font_sg_18, ok ? C_ACCENT : C_WARN);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 6);

  snprintf(b, sizeof(b), T("Mittel %.2f g · Bi %+.2f g"), mean, bias);
  comma(b);
  lv_obj_t *l3 = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(l3, LV_ALIGN_CENTER, 0, 40);
  snprintf(b, sizeof(b), "s %.3f g · n %d · T %d g", sg, m_done, m_tol);
  comma(b);
  lv_obj_t *l4 = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(l4, LV_ALIGN_CENTER, 0, 62);

  // Kleinste prüfbare Toleranz: T_min = 40·s + 10·|Bi|  (aus Cgk = 1,33)
  snprintf(b, sizeof(b), T("kleinste prüfbare Toleranz %.1f g"), m_res.tmin);
  comma(b);
  lv_obj_t *l5 = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(l5, LV_ALIGN_CENTER, 0, 85);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 126);
  lv_obj_t *b1 = ui_btn(row, "Verlauf", BTN_NORMAL);
  lv_obj_add_event_cb(b1, [](lv_event_t *e) { ui_switch_page(page_chart_create()); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *b2 = ui_btn(row, "Fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(b2, [](lv_event_t *e) { ui_go_home(); }, LV_EVENT_CLICKED, NULL);
  return s;
}

// ------------------------------------------------------------
//  Werteverlauf: Einzelwerte über der Nummer, grünes Band = Soll ± 0,1·T
//  (dort muss der Mittelwert liegen, damit Cgk passt), gestrichelt der
//  Mittelwert. Darunter, wann die Waage kalibriert wurde.
// ------------------------------------------------------------
#define MC_W 296
#define MC_H 150
static lv_point_t mc_pts[MSA_MAX], mc_soll[2], mc_mean[2];

static lv_obj_t *mc_line(lv_obj_t *par, lv_point_t *p, int n, lv_color_t c, int w, bool dash) {
  lv_obj_t *l = lv_line_create(par);
  lv_line_set_points(l, p, n);
  lv_obj_set_pos(l, 0, 0);
  lv_obj_set_style_line_width(l, w, 0);
  lv_obj_set_style_line_color(l, c, 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  if (dash) {
    lv_obj_set_style_line_dash_width(l, 6, 0);
    lv_obj_set_style_line_dash_gap(l, 5, 0);
  }
  return l;
}

static lv_obj_t *page_chart_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Werteverlauf", &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);
  char b[64];
  snprintf(b, sizeof(b), T("Soll %d g · Band ± 0,1·T · gestrichelt: Mittel"), m_nominal);
  lv_obj_t *st = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 72);

  // Wertebereich: Band etwas größer als ± 0,1·T, alle Werte sichtbar
  double band = 0.1 * m_tol;
  double lo = m_nominal - band * 1.6, hi = m_nominal + band * 1.6;
  if (m_res.lo < lo) lo = m_res.lo;
  if (m_res.hi > hi) hi = m_res.hi;
  double pad = (hi - lo) * 0.08;
  lo -= pad;
  hi += pad;
  auto Y = [&](double g) { return (lv_coord_t)lround((hi - g) / (hi - lo) * (MC_H - 1)); };

  lv_obj_t *fr = ui_box(s);
  lv_obj_set_size(fr, MC_W, MC_H);
  lv_obj_set_style_border_width(fr, 1, 0);
  lv_obj_set_style_border_color(fr, C_TRACK, 0);
  lv_obj_set_style_radius(fr, 8, 0);
  lv_obj_set_style_clip_corner(fr, true, 0);
  lv_obj_align(fr, LV_ALIGN_CENTER, 0, -6);

  // Band Soll ± 0,1·T
  lv_obj_t *bd = lv_obj_create(fr);
  lv_obj_remove_style_all(bd);
  int y1 = Y(m_nominal + band), y2 = Y(m_nominal - band);
  lv_obj_set_pos(bd, 0, y1);
  lv_obj_set_size(bd, MC_W, y2 - y1 > 1 ? y2 - y1 : 1);
  lv_obj_set_style_bg_color(bd, C_ACCENT, 0);
  lv_obj_set_style_bg_opa(bd, LV_OPA_20, 0);
  lv_obj_clear_flag(bd, LV_OBJ_FLAG_CLICKABLE);

  mc_soll[0] = { 0, Y(m_nominal) };
  mc_soll[1] = { MC_W - 1, Y(m_nominal) };
  mc_line(fr, mc_soll, 2, C_MUTED, 1, false);
  mc_mean[0] = { 0, Y(m_res.mean) };
  mc_mean[1] = { MC_W - 1, Y(m_res.mean) };
  mc_line(fr, mc_mean, 2, C_TEXT, 2, true);
  int n = m_done > 1 ? m_done : 1;
  for (int i = 0; i < m_done; i++) {
    mc_pts[i].x = (lv_coord_t)(6 + i * (MC_W - 13) / (n > 1 ? n - 1 : 1));
    mc_pts[i].y = Y(m_val[i]);
  }
  if (m_done > 1) mc_line(fr, mc_pts, m_done, m_res.ok ? C_ACCENT : C_WARN, 3, false);

  // Skala rechts im Diagramm: obere und untere Bandgrenze
  char g1[16], g2[16];
  int dec = band >= 0.5 ? 1 : 2;  // Bandgrenzen: so genau wie nötig
  snprintf(g1, sizeof(g1), "%.*f", dec, m_nominal + band);
  snprintf(g2, sizeof(g2), "%.*f", dec, m_nominal - band);
  for (char *c = g1; *c; c++) if (*c == '.') *c = ',';
  for (char *c = g2; *c; c++) if (*c == '.') *c = ',';
  lv_obj_t *lt = ui_label(fr, g1, &font_sg_14, C_FAINT);
  lv_obj_align(lt, LV_ALIGN_TOP_RIGHT, -6, y1 - 18 > 2 ? y1 - 18 : 2);
  lv_obj_t *lb = ui_label(fr, g2, &font_sg_14, C_FAINT);
  lv_obj_align(lb, LV_ALIGN_TOP_RIGHT, -6, y2 + 2 < MC_H - 18 ? y2 + 2 : MC_H - 18);

  // Kalibrierung, die bei dieser Prüfung galt
  char d[16], pts[48] = "";
  de_date(d, sizeof(d), scale_cal_date());
  for (int i = 0; i < scale_cal_count(); i++) {
    char one[12];
    snprintf(one, sizeof(one), "%s%d", i ? " · " : "", (int)lroundf(scale_cal_gram(i)));
    strncat(pts, one, sizeof(pts) - strlen(pts) - 1);
  }
  snprintf(b, sizeof(b), T("kalibriert %s · %s g"), d, pts[0] ? pts : "–");
  lv_obj_t *cl = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(cl, LV_ALIGN_CENTER, 0, 86);
  lv_obj_t *fl = ui_label(s, m_file[0] ? m_file : T("nicht gespeichert (SD-Karte?)"), &font_sg_14, C_MUTED);
  lv_obj_align(fl, LV_ALIGN_CENTER, 0, 108);

  lv_obj_t *bk = ui_btn(s, "Zurück", BTN_NORMAL);
  lv_obj_align(bk, LV_ALIGN_CENTER, 0, 148);
  lv_obj_add_event_cb(bk, [](lv_event_t *e) { ui_switch_page(page_result_create()); }, LV_EVENT_CLICKED, NULL);
  return s;
}
