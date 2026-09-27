// ============================================================
//  2K mischen (Werkstatt): Harz/Silikon nach Gewichtsverhältnis
//
//  1. Verhältnis A:B einstellen (B in Teilen je 100 Teile A) und Topfzeit
//  2. Becher auflegen -> Tara, Komponente A eingießen, "A fertig"
//  3. Komponente B: Soll = A · B/100, Ring und Parkpiepser; liegt der
//     Wert ruhig in der Toleranz, geht es von selbst weiter
//  4. Ergebnis mit tatsächlichem Verhältnis; Topfzeit läuft als Timer
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "sound.h"
#include "data.h"
#include "tools.h"
#include <stdio.h>
#include <math.h>

static int m_b = 10;       // Teile B je 100 Teile A
static int m_pot = 20;     // Topfzeit in min (0 = kein Timer)
static float m_a = 0, m_bg = 0;

static const int POT_STEPS[] = { 0, 5, 10, 20, 30, 45, 60, 90 };
#define POT_N (int)(sizeof(POT_STEPS) / sizeof(POT_STEPS[0]))

static lv_obj_t *page_mix_a_create();
static lv_obj_t *page_mix_b_create();
static lv_obj_t *page_mix_done_create();

// "100 : 10" bzw. "1 : 1" für runde Verhältnisse
static void ratio_text(char *b, int len) {
  if (m_b == 100) snprintf(b, len, "1 : 1");
  else if (100 % m_b == 0) snprintf(b, len, "%d : 1", 100 / m_b);
  else snprintf(b, len, "100 : %d", m_b);
}

// ------------------------------------------------------------
//  Schritt 1: Verhältnis und Topfzeit
// ------------------------------------------------------------
static lv_obj_t *ms_ratio, *ms_pot;

static void ms_show() {
  char b[32];
  ratio_text(b, sizeof(b));
  lv_label_set_text(ms_ratio, b);
  if (m_pot > 0) snprintf(b, sizeof(b), T("Topfzeit %d min ›"), m_pot);
  else snprintf(b, sizeof(b), "%s", T("Topfzeit aus ›"));
  lv_label_set_text(lv_obj_get_child(ms_pot, 0), b);
}

static void ms_adj_cb(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  m_b += d;
  if (m_b < 1) m_b = 1;
  if (m_b > 200) m_b = 200;
  sound_play(SND_CLICK);
  ms_show();
}

static void ms_preset_cb(lv_event_t *e) {
  m_b = (int)(intptr_t)lv_event_get_user_data(e);
  sound_play(SND_CLICK);
  ms_show();
}

lv_obj_t *page_mix_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "2K mischen · A : B", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);

  ms_ratio = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(ms_ratio, LV_ALIGN_CENTER, 0, -86);
  lv_obj_t *minus = ui_round_btn(s, "−", NULL);
  lv_obj_align(minus, LV_ALIGN_CENTER, -128, -86);
  lv_obj_add_event_cb(minus, ms_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
  lv_obj_t *plus = ui_round_btn(s, "+", NULL);
  lv_obj_align(plus, LV_ALIGN_CENTER, 128, -86);
  lv_obj_add_event_cb(plus, ms_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

  static const int PRE[4] = { 100, 50, 10, 5 };
  static const char *const PRE_T[4] = { "1:1", "2:1", "10:1", "20:1" };
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, -26);
  for (int i = 0; i < 4; i++) {
    lv_obj_t *c = ui_btn(row, PRE_T[i], BTN_NORMAL);
    lv_obj_set_height(c, 40);
    lv_obj_set_style_pad_hor(c, 12, 0);
    lv_obj_add_event_cb(c, ms_preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PRE[i]);
  }

  ms_pot = ui_btn(s, "", BTN_NORMAL);
  lv_obj_set_height(ms_pot, 42);
  lv_obj_align(ms_pot, LV_ALIGN_CENTER, 0, 32);
  lv_obj_add_event_cb(ms_pot, [](lv_event_t *e) {
    int k = 0;
    while (k < POT_N && POT_STEPS[k] != m_pot) k++;
    m_pot = POT_STEPS[(k + 1) % POT_N];
    sound_play(SND_CLICK);
    ms_show();
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *go = ui_btn(s, "Start", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 30, 0);
  lv_obj_align(go, LV_ALIGN_CENTER, 0, 96);
  lv_obj_add_event_cb(go, [](lv_event_t *e) {
    scale_tare();
    sound_play(SND_TARA);
    ui_switch_page(page_mix_a_create());
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Leeren Becher auflegen, dann Start", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 148);

  ms_show();
  return s;
}

// ------------------------------------------------------------
//  Schritt 2: Komponente A
// ------------------------------------------------------------
static lv_obj_t *ma_weight, *ma_next;

static void ma_timer_cb(lv_timer_t *t) {
  char w[16];
  ui_fmt_weight(w, sizeof(w), scale_net());
  ui_label_update(ma_weight, w);
  if (scale_stable() && scale_net() > 1.0f) lv_obj_clear_state(ma_next, LV_STATE_DISABLED);
  else lv_obj_add_state(ma_next, LV_STATE_DISABLED);
}

static lv_obj_t *page_mix_a_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Komponente A eingießen", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);
  ma_weight = ui_label(s, "0,0", &font_sg_104, C_TEXT);
  lv_obj_align(ma_weight, LV_ALIGN_CENTER, 0, -30);
  lv_obj_t *u = ui_label(s, "g", &font_sg_34, C_MUTED);
  lv_obj_align(u, LV_ALIGN_CENTER, 0, 40);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 110);
  lv_obj_t *tara = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_add_event_cb(tara, [](lv_event_t *e) {
    scale_tare();
    sound_play(SND_TARA);
  }, LV_EVENT_CLICKED, NULL);
  ma_next = ui_btn(row, "A fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(ma_next, [](lv_event_t *e) {
    if (!scale_stable()) return;
    m_a = scale_net();
    scale_tare();  // B wird ab jetzt allein gewogen
    sound_play(SND_SAVE);
    ui_switch_page(page_mix_b_create());
  }, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, ma_timer_cb, 100);
  ma_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Schritt 3: Komponente B mit Ziel
// ------------------------------------------------------------
static lv_obj_t *mb_ring, *mb_weight, *mb_dev;
static float mb_goal, mb_tol;
static uint32_t mb_ok_since;
static int mb_prev = -1;

static void mb_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  char b[32], w[16];
  ui_fmt_weight(w, sizeof(w), g);
  ui_label_update(mb_weight, w);
  int v = mb_goal > 0 ? (int)(g / mb_goal * 1000) : 0;
  if (v < 0) v = 0;
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(mb_ring) != v) lv_arc_set_value(mb_ring, v);

  float d = g - mb_goal;
  int state = d < -mb_tol ? 0 : (d <= mb_tol ? 1 : 2);
  if (state == 0) {
    ui_fmt_1dec(w, sizeof(w), -d);
    snprintf(b, sizeof(b), T("noch %s g"), w);
  } else if (state == 1) {
    snprintf(b, sizeof(b), "%s", T("passt"));
  } else {
    ui_fmt_1dec(w, sizeof(w), d);
    snprintf(b, sizeof(b), T("%s g zu viel"), w);
  }
  ui_label_update(mb_dev, b);
  if (state != mb_prev) {
    mb_prev = state;
    lv_color_t c = state == 1 ? C_ACCENT : (state == 2 ? C_DANGER : C_TEXT2);
    lv_obj_set_style_text_color(mb_dev, c, 0);
    lv_obj_set_style_arc_color(mb_ring, state == 2 ? C_DANGER : C_ACCENT, LV_PART_INDICATOR);
  }
  sound_parking(g, mb_goal, mb_tol);

  // ruhig im Ziel -> nach 1,5 s fertig
  uint32_t now = lv_tick_get();
  if (state == 1 && scale_stable()) {
    if (!mb_ok_since) mb_ok_since = now;
    if (now - mb_ok_since > 1500) {
      m_bg = g;
      ui_switch_page(page_mix_done_create());
    }
  } else {
    mb_ok_since = 0;
  }
}

static lv_obj_t *page_mix_b_create() {
  lv_obj_t *s = ui_screen_create();
  mb_goal = m_a * m_b / 100.0f;
  mb_tol = mb_goal * 0.01f;  // 1 %, mindestens 0,1 g
  if (mb_tol < 0.1f) mb_tol = 0.1f;
  mb_ring = ui_ring(s, 396);

  char b[40], w[16];
  ui_fmt_1dec(w, sizeof(w), mb_goal);
  snprintf(b, sizeof(b), T("Komponente B · Ziel %s g"), w);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);

  mb_weight = ui_label(s, "0,0", &font_sg_80, C_TEXT);
  lv_obj_align(mb_weight, LV_ALIGN_CENTER, 0, -30);
  mb_dev = ui_label(s, "", &font_sg_34, C_TEXT2);
  lv_obj_align(mb_dev, LV_ALIGN_CENTER, 0, 40);

  lv_obj_t *fin = ui_btn(s, "Fertig", BTN_NORMAL);
  lv_obj_align(fin, LV_ALIGN_CENTER, 0, 112);
  lv_obj_add_event_cb(fin, [](lv_event_t *e) {
    m_bg = scale_net();
    ui_switch_page(page_mix_done_create());
  }, LV_EVENT_CLICKED, NULL);

  mb_prev = -1;
  mb_ok_since = 0;
  sound_parking_reset();
  ui_page_timer(s, mb_timer_cb, 100);
  mb_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Schritt 4: Ergebnis, Topfzeit starten
// ------------------------------------------------------------
static lv_obj_t *page_mix_done_create() {
  lv_obj_t *s = ui_screen_create();
  sound_parking_reset();
  sound_play(SND_DONE);

  char b[64], a[16], c[16], r[16];
  lv_obj_t *t = ui_label(s, "Gemischt", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);

  ui_fmt_1dec(a, sizeof(a), m_a + m_bg);
  snprintf(b, sizeof(b), "%s g", a);
  lv_obj_t *tot = ui_label(s, b, &font_sg_34, C_TEXT);
  lv_obj_align(tot, LV_ALIGN_CENTER, 0, -84);

  ui_fmt_1dec(a, sizeof(a), m_a);
  ui_fmt_1dec(c, sizeof(c), m_bg);
  snprintf(b, sizeof(b), T("A %s g · B %s g"), a, c);
  lv_obj_t *ab = ui_label(s, b, &font_sg_18, C_TEXT2);
  lv_obj_align(ab, LV_ALIGN_CENTER, 0, -40);

  // tatsächliches Verhältnis in Teilen B je 100 A
  float real = m_a > 0 ? m_bg / m_a * 100.0f : 0;
  ui_fmt_1dec(r, sizeof(r), real);
  char want[16];
  ratio_text(want, sizeof(want));
  snprintf(b, sizeof(b), T("100 : %s (Soll %s)"), r, want);
  bool ok = fabsf(real - m_b) <= m_b * 0.02f + 0.05f;
  lv_obj_t *rl = ui_label(s, b, &font_sg_18, ok ? C_ACCENT : C_WARN);
  lv_obj_align(rl, LV_ALIGN_CENTER, 0, -10);

  // Topfzeit als Küchentimer
  lv_obj_t *pl = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(pl, LV_ALIGN_CENTER, 0, 28);
  if (m_pot > 0) {
    int i = timer_free();
    if (i >= 0) {
      timer_start(i, (uint32_t)m_pot * 60, T("Topfzeit"));
      snprintf(b, sizeof(b), T("Topfzeit %d min läuft (%s)"), m_pot, timer_name(i));
    } else {
      snprintf(b, sizeof(b), "%s", T("Kein Timer frei für die Topfzeit"));
    }
    lv_label_set_text(pl, b);
  }

  char note[48], rt[16];
  ratio_text(rt, sizeof(rt));
  snprintf(note, sizeof(note), T("2K %s · A %s g · B %s g"), rt, a, c);
  log_add(m_a + m_bg, note);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 96);
  lv_obj_t *again = ui_btn(row, "Nochmal", BTN_PRIMARY);
  lv_obj_add_event_cb(again, [](lv_event_t *e) { ui_switch_page(page_mix_create()); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *home = ui_btn(row, "Zu Wiegen", BTN_NORMAL);
  lv_obj_add_event_cb(home, [](lv_event_t *e) { ui_go_home(); }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Im Protokoll gespeichert", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 150);
  return s;
}
