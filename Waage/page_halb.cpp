// ============================================================
//  Spiel "Halbe-Halbe" (wie das Teil-Spiel bei "Schlag den Star"):
//  Jeder bekommt ein Lebensmittel (Banane, Brot, Käse …), die Waage
//  wiegt es verdeckt, dann wird es geteilt und eine Hälfte aufgelegt.
//  Wer am nächsten an 50 % liegt, gewinnt. Die Anzeige bleibt bis zur
//  Auflösung verdeckt.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "sound.h"
#include "data.h"
#include "hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ITEM_MIN_G 5.0f
#define EMPTY_G 3.0f
#define HOLD_MS 1000   // so lange muss ein Teil ruhig liegen

static int h_turn = 0;
static float h_whole[PLAYER_MAX], h_half[PLAYER_MAX];

static lv_obj_t *page_halb_turn_create();
static lv_obj_t *page_halb_result_create();

static const char *hname(int nth) {
  return player_name(players_pick_index(nth));
}

// Abweichung von der perfekten Hälfte in Prozentpunkten
static float h_dev(int i) {
  return h_whole[i] > 0 ? fabsf(h_half[i] / h_whole[i] * 100.0f - 50.0f) : 99.0f;
}

// ------------------------------------------------------------
//  Einrichten
// ------------------------------------------------------------
static lv_obj_t *hs_players;

static void hs_show() {
  char b[96];
  b[0] = 0;
  for (int i = 0; i < players_pick_count() && i < 6; i++) {
    strncat(b, i ? ", " : "", sizeof(b) - strlen(b) - 1);
    strncat(b, hname(i), sizeof(b) - strlen(b) - 1);
  }
  if (!b[0]) snprintf(b, sizeof(b), "%s", T("niemand gewählt"));
  lv_label_set_text(hs_players, b);
}

lv_obj_t *page_halb_create() {
  static bool loaded = false;
  if (!loaded) {
    players_load();
    loaded = true;
  }
  players_pick_default();
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Halbe-Halbe", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  hs_players = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_set_width(hs_players, 280);
  lv_label_set_long_mode(hs_players, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(hs_players, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(hs_players, LV_ALIGN_CENTER, 0, -96);

  lv_obj_t *rules = ui_label(s,
                             "Jeder bekommt ein Lebensmittel.\n"
                             "Ganz auflegen, abnehmen, teilen,\n"
                             "eine Hälfte auflegen.\n"
                             "Am nächsten an 50 % gewinnt.",
                             &font_sg_14, C_TEXT2);
  lv_obj_set_style_text_align(rules, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_line_space(rules, 4, 0);
  lv_obj_align(rules, LV_ALIGN_CENTER, 0, -18);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 76);
  lv_obj_t *nb = ui_btn(row, "Spieler ›", BTN_NORMAL);
  lv_obj_add_event_cb(nb, [](lv_event_t *e) { ui_switch_page(page_players_create(page_halb_create)); },
                      LV_EVENT_CLICKED, NULL);
  lv_obj_t *go = ui_btn(row, "Los", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 34, 0);
  lv_obj_add_event_cb(go, [](lv_event_t *e) {
    if (players_pick_count() < 1) {
      sound_play(SND_WARN);
      return;
    }
    h_turn = 0;
    memset(h_whole, 0, sizeof(h_whole));
    memset(h_half, 0, sizeof(h_half));
    ui_switch_page(page_halb_turn_create());
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Messer und Brett bereitlegen", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 150);
  hs_show();
  return s;
}

// ------------------------------------------------------------
//  Ein Spieler teilt
//  0 = Waage leeren, 1 = Ganzes auflegen, 2 = abnehmen und teilen,
//  3 = eine Hälfte auflegen, 4 = Auflösung
// ------------------------------------------------------------
static lv_obj_t *ht_big, *ht_state, *ht_sub, *ht_next, *ht_ring;
static int ht_st;
static uint32_t ht_since;

static bool held(bool cond) {  // Bedingung liegt ruhig HOLD_MS lang an
  uint32_t now = hal_millis();
  if (!cond || !scale_stable()) {
    ht_since = 0;
    return false;
  }
  if (!ht_since) ht_since = now;
  return now - ht_since >= HOLD_MS;
}

static void ht_reveal() {
  char b[48], p[16], d[16];
  float pct = h_half[h_turn] / h_whole[h_turn] * 100.0f;
  ui_fmt_1dec(p, sizeof(p), pct);
  snprintf(b, sizeof(b), "%s %%", p);
  lv_obj_set_style_text_font(ht_big, &font_sg_80, 0);
  lv_obj_set_style_text_color(ht_big, h_dev(h_turn) <= 1.0f ? C_ACCENT : C_TEXT, 0);
  lv_label_set_text(ht_big, b);
  ui_fmt_1dec(d, sizeof(d), h_dev(h_turn));
  snprintf(b, sizeof(b), T("%s %%-Punkte daneben"), d);
  lv_label_set_text(ht_state, b);
  char w1[16], w2[16];
  ui_fmt_1dec(w1, sizeof(w1), h_half[h_turn]);
  ui_fmt_1dec(w2, sizeof(w2), h_whole[h_turn]);
  snprintf(b, sizeof(b), T("Hälfte %s g von %s g"), w1, w2);
  lv_label_set_text(ht_sub, b);
  // Ring zeigt den Anteil: 50 % = halber Kreis
  lv_arc_set_value(ht_ring, (int)(pct * 10));
  lv_obj_set_style_arc_color(ht_ring, h_dev(h_turn) <= 1.0f ? C_ACCENT : C_WARN, LV_PART_INDICATOR);
  lv_obj_clear_flag(ht_next, LV_OBJ_FLAG_HIDDEN);
  sound_play(h_dev(h_turn) <= 1.0f ? SND_DONE : SND_REACHED);
}

static void ht_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  switch (ht_st) {
    case 0:  // leer und tarieren
      ui_label_update(ht_state, T("Waage leeren"));
      if (held((g < EMPTY_G && g > -EMPTY_G) || fabsf(scale_gross()) < 5.0f)) {
        scale_tare();
        ht_st = 1;
        ht_since = 0;
      }
      break;
    case 1:
      ui_label_update(ht_state, T("Ganzes Stück auflegen"));
      if (held(g > ITEM_MIN_G)) {
        h_whole[h_turn] = g;
        sound_play(SND_TICK);
        ht_st = 2;
        ht_since = 0;
      }
      break;
    case 2:
      ui_label_update(ht_state, T("Abnehmen und teilen"));
      if (held(g < EMPTY_G)) {
        ht_st = 3;
        ht_since = 0;
      }
      break;
    case 3:
      ui_label_update(ht_state, T("Eine Hälfte auflegen"));
      if (held(g > ITEM_MIN_G * 0.5f)) {
        h_half[h_turn] = g;
        ht_st = 4;
        sound_play(SND_DRUM);
        ht_since = hal_millis();
      }
      break;
    case 4:  // nach dem Trommelwirbel auflösen
      if (ht_since && hal_millis() - ht_since > 1400) {
        ht_since = 0;
        ht_reveal();
      }
      break;
  }
}

static lv_obj_t *page_halb_turn_create() {
  lv_obj_t *s = ui_screen_create();
  ht_ring = ui_ring(s, 396);
  char b[40];
  snprintf(b, sizeof(b), T("Zug %d / %d"), h_turn + 1, players_pick_count());
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_t *n = ui_label(s, hname(h_turn), &font_sg_34, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -104);

  ht_big = ui_label(s, "? ? ?", &font_sg_80, C_FAINT);
  lv_obj_align(ht_big, LV_ALIGN_CENTER, 0, -26);
  ht_state = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(ht_state, LV_ALIGN_CENTER, 0, 40);
  ht_sub = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(ht_sub, LV_ALIGN_CENTER, 0, 72);

  ht_next = ui_btn(s, h_turn + 1 >= players_pick_count() ? "Ergebnis" : "Nächster", BTN_PRIMARY);
  lv_obj_align(ht_next, LV_ALIGN_CENTER, 0, 118);
  lv_obj_add_flag(ht_next, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(ht_next, [](lv_event_t *e) {
    h_turn++;
    if (h_turn >= players_pick_count()) ui_switch_page(page_halb_result_create());
    else ui_switch_page(page_halb_turn_create());
  }, LV_EVENT_CLICKED, NULL);

  ht_st = 0;
  ht_since = 0;
  ui_page_timer(s, ht_timer_cb, 100);
  ht_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Endstand
// ------------------------------------------------------------
static lv_obj_t *page_halb_result_create() {
  lv_obj_t *s = ui_screen_create();
  int n = players_pick_count();
  int order[PLAYER_MAX];
  for (int i = 0; i < n; i++) order[i] = i;
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && h_dev(order[j]) < h_dev(order[j - 1]); j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }

  lv_obj_t *t = ui_label(s, "Endstand", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_t *w = ui_label(s, n > 0 ? hname(order[0]) : "-", &font_sg_34, C_ACCENT);
  lv_obj_align(w, LV_ALIGN_CENTER, 0, -100);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 260, 140);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, 4);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  char b[48], p[16];
  for (int k = 0; k < n; k++) {
    int i = order[k];
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 260, 30);
    ui_fmt_1dec(p, sizeof(p), h_half[i] / (h_whole[i] > 0 ? h_whole[i] : 1) * 100.0f);
    snprintf(b, sizeof(b), "%d. %.10s · %s %%", k + 1, hname(i), p);
    lv_obj_t *l = ui_label(row, b, &font_sg_14, k == 0 ? C_ACCENT : C_TEXT2);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    ui_fmt_1dec(p, sizeof(p), h_dev(i));
    snprintf(b, sizeof(b), "±%s", p);
    lv_obj_t *r = ui_label(row, b, &font_sg_14, k == 0 ? C_ACCENT : C_MUTED);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
    score_add("Halbieren", hname(i), h_dev(i), "%");
  }
  if (n > 0) {
    char note[48];
    snprintf(note, sizeof(note), T("Halbe-Halbe: Sieger %s"), hname(order[0]));
    log_add(h_half[order[0]], note);
  }

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 118);
  lv_obj_t *again = ui_btn(row, "Nochmal", BTN_PRIMARY);
  lv_obj_add_event_cb(again, [](lv_event_t *e) { ui_switch_page(page_halb_create()); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *home = ui_btn(row, "Zu Wiegen", BTN_NORMAL);
  lv_obj_add_event_cb(home, [](lv_event_t *e) { ui_go_home(); }, LV_EVENT_CLICKED, NULL);
  sound_play(SND_DONE);
  return s;
}
