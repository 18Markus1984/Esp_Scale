// ============================================================
//  Münzzähler (Werkstatt): eine Münzsorte auflegen -> Stückzahl und
//  Betrag. Mehrere Sorten nacheinander ergeben den Kassensturz
//  ("+ Summe"). Gewichte der Euro-Münzen laut EZB.
//  Gemischte Sorten lassen sich über das Gewicht nicht sicher trennen,
//  deshalb immer nur eine Sorte auf einmal.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "scale.h"
#include "sound.h"
#include "data.h"
#include <stdio.h>
#include <math.h>

typedef struct {
  const char *name;
  int cents;
  float grams;
} coin_t;

static const coin_t COINS[] = {
  { "2 €", 200, 8.50f },   { "1 €", 100, 7.50f },   { "50 ct", 50, 7.80f }, { "20 ct", 20, 5.74f },
  { "10 ct", 10, 4.10f },  { "5 ct", 5, 3.92f },    { "2 ct", 2, 3.06f },   { "1 ct", 1, 2.30f },
};
#define COIN_N (int)(sizeof(COINS) / sizeof(COINS[0]))

static int c_coin = 0;
static long c_sum_cents = 0;  // Kassensturz über mehrere Sorten
static int c_sum_n = 0;

static lv_obj_t *page_coin_count_create();

static void fmt_eur(char *b, int len, long cents) {
  snprintf(b, len, "%ld,%02ld €", cents / 100, labs(cents % 100));
}

// ------------------------------------------------------------
//  Sorte wählen (letzter Eintrag: Summe löschen)
// ------------------------------------------------------------
static void coin_pick_cb(int index) {
  if (index >= COIN_N) {  // Summe zurücksetzen
    c_sum_cents = 0;
    c_sum_n = 0;
    sound_play(SND_CLICK);
    ui_switch_page(page_muenzen_create());
    return;
  }
  c_coin = index;
  ui_switch_page(page_coin_count_create());
}

lv_obj_t *page_muenzen_create() {
  static ui_list_item_t items[COIN_N + 1];
  static char subs[COIN_N + 1][32];
  for (int i = 0; i < COIN_N; i++) {
    char w[12];
    fmt_num(w, sizeof(w), COINS[i].grams, 2);
    snprintf(subs[i], sizeof(subs[i]), T("%s g je Münze"), w);
    items[i].title = COINS[i].name;
    items[i].sub = subs[i];
    items[i].icon = NULL;
  }
  char e[16];
  fmt_eur(e, sizeof(e), c_sum_cents);
  snprintf(subs[COIN_N], sizeof(subs[COIN_N]), T("Summe %s"), e);
  items[COIN_N].title = T("Summe löschen");
  items[COIN_N].sub = subs[COIN_N];
  items[COIN_N].icon = NULL;

  lv_obj_t *s = ui_screen_create();
  ui_curved_list(s, items, COIN_N + 1, coin_pick_cb);
  lv_obj_t *t = ui_label(s, "Münzen · Sorte", &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return s;
}

// ------------------------------------------------------------
//  Zählen
// ------------------------------------------------------------
static lv_obj_t *cc_count, *cc_value, *cc_info, *cc_sum, *cc_toast, *cc_add;

static int cc_n() {
  float n = scale_net() / COINS[c_coin].grams;
  return n > 0 ? (int)lroundf(n) : 0;
}

static void cc_timer_cb(lv_timer_t *t) {
  char b[48], w[16], e[16];
  int n = cc_n();
  snprintf(b, sizeof(b), "%d", n);
  ui_label_update(cc_count, b);
  fmt_eur(e, sizeof(e), (long)n * COINS[c_coin].cents);
  snprintf(b, sizeof(b), "= %s", e);
  ui_label_update(cc_value, b);

  float g = scale_net();
  float frac = fabsf(g / COINS[c_coin].grams - n);
  ui_fmt_weight(w, sizeof(w), g);
  if (n > 0 && frac > 0.3f) {  // passt nicht zur Sorte: andere Münze dabei?
    snprintf(b, sizeof(b), T("%s g · unsicher, Sorte prüfen"), w);
    lv_obj_set_style_text_color(cc_info, C_WARN, 0);
  } else {
    snprintf(b, sizeof(b), "%s g", w);
    lv_obj_set_style_text_color(cc_info, C_MUTED, 0);
  }
  ui_label_update(cc_info, b);

  fmt_eur(e, sizeof(e), c_sum_cents);
  snprintf(b, sizeof(b), T("Summe %s"), e);
  ui_label_update(cc_sum, b);

  if (n > 0 && scale_stable()) lv_obj_clear_state(cc_add, LV_STATE_DISABLED);
  else lv_obj_add_state(cc_add, LV_STATE_DISABLED);
}

static void cc_add_cb(lv_event_t *e) {
  if (!scale_stable()) return;
  int n = cc_n();
  if (n <= 0) return;
  long cents = (long)n * COINS[c_coin].cents;
  c_sum_cents += cents;
  c_sum_n += n;
  char note[48], eur[16];
  fmt_eur(eur, sizeof(eur), cents);
  snprintf(note, sizeof(note), T("Münzen: %d × %s = %s"), n, COINS[c_coin].name, eur);
  log_add(scale_net(), note);
  sound_play(SND_SAVE);
  ui_toast_show(cc_toast, "Zur Summe addiert", C_ACCENT);
}

static lv_obj_t *page_coin_count_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, COINS[c_coin].name, &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  cc_count = ui_label(s, "0", &font_sg_104, C_TEXT);
  lv_obj_align(cc_count, LV_ALIGN_CENTER, 0, -60);
  lv_obj_t *st = ui_label(s, "Stück", &font_sg_18, C_MUTED);
  lv_obj_align(st, LV_ALIGN_CENTER, 0, -2);
  cc_value = ui_label(s, "", &font_sg_34, C_ACCENT);
  lv_obj_align(cc_value, LV_ALIGN_CENTER, 0, 34);
  cc_info = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(cc_info, LV_ALIGN_CENTER, 0, 66);

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
  cc_add = ui_btn(row, "+ Summe", BTN_PRIMARY);
  lv_obj_add_event_cb(cc_add, cc_add_cb, LV_EVENT_CLICKED, NULL);

  cc_sum = ui_label(s, "", &font_sg_14, C_TEXT2);
  lv_obj_align(cc_sum, LV_ALIGN_CENTER, 0, 156);
  cc_toast = ui_toast_create(s);
  lv_obj_align(cc_toast, LV_ALIGN_CENTER, 0, 66);

  ui_page_timer(s, cc_timer_cb, 100);
  cc_timer_cb(NULL);
  return s;
}
