// ============================================================
//  Tassen & Löffel (Küche): Umrechnung für amerikanische Rezepte
//  Zutat wählen -> das Aufgelegte in Tassen / EL / TL. Mit Ziel in
//  ¼-Tassen-Schritten wird wie beim Ziel-Modus abgewogen.
//  1 Tasse (US cup) = 236,6 ml, 1 EL = 1/16 Tasse, 1 TL = 1/48 Tasse.
//  Gramm je Tasse: übliche Küchenwerte, locker eingefüllt.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "scale.h"
#include "sound.h"
#include <stdio.h>
#include <math.h>

typedef struct {
  const char *name;
  float g_cup;  // Gramm je Tasse
} cup_t;

static const cup_t CUPS[] = {
  { "Mehl", 125 },           { "Zucker", 200 },       { "Puderzucker", 120 }, { "Brauner Zucker", 213 },
  { "Butter", 227 },         { "Milch", 240 },        { "Wasser", 237 },      { "Öl", 218 },
  { "Honig", 340 },          { "Reis", 185 },         { "Haferflocken", 90 }, { "Kakao", 85 },
  { "Nüsse gehackt", 120 },  { "Schokotropfen", 170 },
};
#define CUP_N (int)(sizeof(CUPS) / sizeof(CUPS[0]))

static int c_idx = 0;
static int c_goal_q = 0;  // Ziel in Vierteltassen (0 = kein Ziel)

static lv_obj_t *page_cups_measure_create();

// 1,37 -> "1 ¼" (auf Viertel gerundet), 0,1 -> "0"
static void cups_frac(char *b, int len, float cups) {
  int q = (int)lroundf(cups * 4);
  int whole = q / 4, rest = q % 4;
  static const char *const FR[4] = { "", "¼", "½", "¾" };
  if (whole && rest) snprintf(b, len, "%d %s", whole, FR[rest]);
  else if (whole) snprintf(b, len, "%d", whole);
  else if (rest) snprintf(b, len, "%s", FR[rest]);
  else snprintf(b, len, "0");
}

// ------------------------------------------------------------
//  Zutat wählen
// ------------------------------------------------------------
static void pick_cb(int index) {
  c_idx = index;
  c_goal_q = 0;
  ui_switch_page(page_cups_measure_create());
}

lv_obj_t *page_tassen_create() {
  static ui_list_item_t items[CUP_N];
  static char subs[CUP_N][28];
  for (int i = 0; i < CUP_N; i++) {
    snprintf(subs[i], sizeof(subs[i]), T("1 Tasse = %d g"), (int)CUPS[i].g_cup);
    items[i].title = CUPS[i].name;
    items[i].sub = subs[i];
    items[i].icon = NULL;
  }
  lv_obj_t *s = ui_screen_create();
  ui_curved_list(s, items, CUP_N, pick_cb);
  lv_obj_t *t = ui_label(s, "Tassen & Löffel · Zutat", &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return s;
}

// ------------------------------------------------------------
//  Messen
// ------------------------------------------------------------
static lv_obj_t *cm_ring, *cm_weight, *cm_cups, *cm_spoons, *cm_goal, *cm_rest;
static int cm_prev = -1;

static void cm_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  float gc = CUPS[c_idx].g_cup;
  float cups = g / gc;
  char b[48], w[16], f[16];

  ui_fmt_weight(w, sizeof(w), g);
  snprintf(b, sizeof(b), "%s g", w);
  ui_label_update(cm_weight, b);

  cups_frac(f, sizeof(f), cups > 0 ? cups : 0);
  snprintf(b, sizeof(b), T("ca. %s Tassen"), f);
  if (cups > 0 && cups < 0.125f) snprintf(b, sizeof(b), "%s", T("unter 1/8 Tasse"));
  ui_label_update(cm_cups, b);

  float el = cups * 16, tl = cups * 48;
  if (g < 0.5f) snprintf(b, sizeof(b), "%s", T("Zutat auflegen"));
  else if (el < 1) {
    ui_fmt_1dec(w, sizeof(w), tl);
    snprintf(b, sizeof(b), T("= %s TL"), w);
  } else {
    ui_fmt_1dec(w, sizeof(w), el);
    snprintf(b, sizeof(b), T("= %s EL · %d TL"), w, (int)lroundf(tl));
  }
  ui_label_update(cm_spoons, b);

  // Ziel
  if (c_goal_q > 0) {
    float goal = c_goal_q / 4.0f * gc;
    cups_frac(f, sizeof(f), c_goal_q / 4.0f);
    snprintf(b, sizeof(b), T("Ziel %s Tassen"), f);
    ui_label_update(cm_goal, b);
    float tol = goal * 0.02f < 1 ? 1 : goal * 0.02f;
    float d = goal - g;
    int state = d > tol ? 0 : (d >= -tol ? 1 : 2);
    if (state == 0) {
      ui_fmt_1dec(w, sizeof(w), d);
      snprintf(b, sizeof(b), T("noch %s g"), w);
    } else if (state == 1) {
      snprintf(b, sizeof(b), "%s", T("passt"));
    } else {
      ui_fmt_1dec(w, sizeof(w), -d);
      snprintf(b, sizeof(b), T("%s g zu viel"), w);
    }
    ui_label_update(cm_rest, b);
    if (state != cm_prev) {
      cm_prev = state;
      lv_obj_set_style_text_color(cm_rest, state == 1 ? C_ACCENT : (state == 2 ? C_DANGER : C_TEXT2), 0);
      lv_obj_set_style_arc_color(cm_ring, state == 2 ? C_DANGER : C_ACCENT, LV_PART_INDICATOR);
    }
    int v = (int)(g / goal * 1000);
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    if (lv_arc_get_value(cm_ring) != v) lv_arc_set_value(cm_ring, v);
    sound_parking(g, goal, tol);
  } else {
    ui_label_update(cm_goal, T("Ziel: aus"));
    ui_label_update(cm_rest, "");
    if (lv_arc_get_value(cm_ring) != 0) lv_arc_set_value(cm_ring, 0);
    cm_prev = -1;
  }
}

static void cm_goal_cb(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  c_goal_q += d;
  if (c_goal_q < 0) c_goal_q = 0;
  if (c_goal_q > 40) c_goal_q = 40;  // 10 Tassen
  sound_parking_reset();
  sound_play(SND_CLICK);
}

static lv_obj_t *page_cups_measure_create() {
  lv_obj_t *s = ui_screen_create();
  cm_ring = ui_ring(s, 396);

  lv_obj_t *t = ui_label(s, CUPS[c_idx].name, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);

  cm_weight = ui_label(s, "", &font_sg_24, C_TEXT2);
  lv_obj_align(cm_weight, LV_ALIGN_CENTER, 0, -104);
  cm_cups = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(cm_cups, LV_ALIGN_CENTER, 0, -60);
  cm_spoons = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_align(cm_spoons, LV_ALIGN_CENTER, 0, -22);

  lv_obj_t *minus = ui_round_btn(s, "−", NULL);
  lv_obj_align(minus, LV_ALIGN_CENTER, -120, 30);
  lv_obj_add_event_cb(minus, cm_goal_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
  lv_obj_t *plus = ui_round_btn(s, "+", NULL);
  lv_obj_align(plus, LV_ALIGN_CENTER, 120, 30);
  lv_obj_add_event_cb(plus, cm_goal_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
  cm_goal = ui_label(s, "", &font_sg_18, C_TEXT);
  lv_obj_align(cm_goal, LV_ALIGN_CENTER, 0, 30);
  cm_rest = ui_label(s, "", &font_sg_18, C_TEXT2);
  lv_obj_align(cm_rest, LV_ALIGN_CENTER, 0, 64);

  lv_obj_t *tara = ui_btn(s, "Tara", BTN_NORMAL);
  lv_obj_set_style_pad_hor(tara, 24, 0);
  lv_obj_align(tara, LV_ALIGN_CENTER, 0, 120);
  lv_obj_add_event_cb(tara, [](lv_event_t *e) {
    scale_tare();
    sound_play(SND_TARA);
  }, LV_EVENT_CLICKED, NULL);

  cm_prev = -1;
  sound_parking_reset();
  ui_page_timer(s, cm_timer_cb, 100);
  cm_timer_cb(NULL);
  return s;
}
