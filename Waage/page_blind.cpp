// ============================================================
//  Spiel "Blindgießen": Jeder gießt reihum eine Zielmenge in ein
//  Glas, ohne die Anzeige zu sehen. Erst am Ende wird aufgelöst.
//  Spieler kommen aus der gemeinsamen Liste (wie Schätz-/Trinkspiel),
//  Ergebnisse landen in der Bestenliste (Spiel "Blindgiessen").
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
#include <stdlib.h>
#include <math.h>

#define GLASS_MIN_G 20.0f

static int b_goal = 150;
static int b_turn = 0;
static float b_poured[PLAYER_MAX];

static lv_obj_t *page_blind_turn_create();
static lv_obj_t *page_blind_result_create();

static const char *bname(int nth) {
  return player_name(players_pick_index(nth));
}

// ------------------------------------------------------------
//  Einrichten
// ------------------------------------------------------------
static lv_obj_t *bs_players, *bs_goal;

static void bs_show() {
  char b[96];
  b[0] = 0;
  for (int i = 0; i < players_pick_count() && i < 6; i++) {
    strncat(b, i ? ", " : "", sizeof(b) - strlen(b) - 1);
    strncat(b, bname(i), sizeof(b) - strlen(b) - 1);
  }
  if (!b[0]) snprintf(b, sizeof(b), "%s", T("niemand gewählt"));
  lv_label_set_text(bs_players, b);
  snprintf(b, sizeof(b), "%d g", b_goal);
  lv_label_set_text(bs_goal, b);
}

static void bs_adj_cb(lv_event_t *e) {
  b_goal += (int)(intptr_t)lv_event_get_user_data(e);
  if (b_goal < 30) b_goal = 30;
  if (b_goal > 1000) b_goal = 1000;
  sound_play(SND_CLICK);
  bs_show();
}

lv_obj_t *page_blind_create() {
  static bool loaded = false;
  if (!loaded) {
    players_load();
    loaded = true;
  }
  players_pick_default();
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Blindgießen", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  bs_players = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_set_width(bs_players, 280);
  lv_label_set_long_mode(bs_players, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(bs_players, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(bs_players, LV_ALIGN_CENTER, 0, -98);

  lv_obj_t *gl = ui_label(s, "Zielmenge", &font_sg_14, C_MUTED);
  lv_obj_align(gl, LV_ALIGN_CENTER, 0, -58);
  bs_goal = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(bs_goal, LV_ALIGN_CENTER, 0, -22);
  lv_obj_t *minus = ui_round_btn(s, "−", NULL);
  lv_obj_align(minus, LV_ALIGN_CENTER, -120, -22);
  lv_obj_add_event_cb(minus, bs_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-10);
  lv_obj_t *plus = ui_round_btn(s, "+", NULL);
  lv_obj_align(plus, LV_ALIGN_CENTER, 120, -22);
  lv_obj_add_event_cb(plus, bs_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)10);

  lv_obj_t *rnd = ui_btn(s, "Zufall", BTN_NORMAL);
  lv_obj_set_height(rnd, 40);
  lv_obj_align(rnd, LV_ALIGN_CENTER, 0, 36);
  lv_obj_add_event_cb(rnd, [](lv_event_t *e) {
    b_goal = 50 + (int)(hal_millis() % 26) * 10;  // 50 … 300 g
    sound_play(SND_CLICK);
    bs_show();
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 100);
  lv_obj_t *nb = ui_btn(row, "Spieler ›", BTN_NORMAL);
  lv_obj_add_event_cb(nb, [](lv_event_t *e) { ui_switch_page(page_players_create(page_blind_create)); },
                      LV_EVENT_CLICKED, NULL);
  lv_obj_t *go = ui_btn(row, "Los", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 34, 0);
  lv_obj_add_event_cb(go, [](lv_event_t *e) {
    if (players_pick_count() < 1) {
      sound_play(SND_WARN);
      return;
    }
    b_turn = 0;
    memset(b_poured, 0, sizeof(b_poured));
    ui_switch_page(page_blind_turn_create());
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Ein Glas für alle, Anzeige bleibt verdeckt", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 152);
  bs_show();
  return s;
}

// ------------------------------------------------------------
//  Ein Spieler gießt
//  0 = Waage leeren, 1 = leeres Glas auflegen, 2 = gießen
// ------------------------------------------------------------
static lv_obj_t *bt_state, *bt_done;
static int bt_st;

static void bt_to_pour() {
  scale_tare();
  sound_play(SND_TARA);
  bt_st = 2;
}

static void bt_timer_cb(lv_timer_t *t) {
  char b[48];
  switch (bt_st) {
    case 0:
      ui_label_update(bt_state, T("Waage leeren"));
      if (fabsf(scale_gross()) < 5.0f && scale_stable()) bt_st = 1;
      break;
    case 1:
      ui_label_update(bt_state, T("Leeres Glas auflegen"));
      if (scale_gross() > GLASS_MIN_G && scale_stable()) bt_to_pour();
      break;
    case 2:
      snprintf(b, sizeof(b), T("Gieß %d g ein"), b_goal);
      ui_label_update(bt_state, b);
      break;
  }
  if (bt_st == 2 && scale_stable() && scale_net() > 1.0f) lv_obj_clear_state(bt_done, LV_STATE_DISABLED);
  else lv_obj_add_state(bt_done, LV_STATE_DISABLED);
}

static lv_obj_t *page_blind_turn_create() {
  lv_obj_t *s = ui_screen_create();
  char b[48];
  snprintf(b, sizeof(b), T("Zug %d / %d"), b_turn + 1, players_pick_count());
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_t *n = ui_label(s, bname(b_turn), &font_sg_34, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -104);

  lv_obj_t *q = ui_label(s, "? ? ?", &font_sg_80, C_FAINT);
  lv_obj_align(q, LV_ALIGN_CENTER, 0, -26);
  bt_state = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(bt_state, LV_ALIGN_CENTER, 0, 42);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 108);
  lv_obj_t *tara = ui_btn(row, "Tara", BTN_NORMAL);  // Notausgang, falls der Nullpunkt nicht passt
  lv_obj_add_event_cb(tara, [](lv_event_t *e) { bt_to_pour(); }, LV_EVENT_CLICKED, NULL);
  bt_done = ui_btn(row, "Fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(bt_done, [](lv_event_t *e) {
    if (bt_st != 2 || !scale_stable()) return;
    b_poured[b_turn] = scale_net();
    sound_play(SND_SAVE);
    b_turn++;
    if (b_turn >= players_pick_count()) ui_switch_page(page_blind_result_create());
    else ui_switch_page(page_blind_turn_create());
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Danach Glas leeren", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 156);

  bt_st = 0;
  ui_page_timer(s, bt_timer_cb, 100);
  bt_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Auflösung (mit Trommelwirbel)
// ------------------------------------------------------------
static lv_obj_t *br_list, *br_win;
static uint32_t br_start;
static bool br_shown;

static void br_timer_cb(lv_timer_t *t) {
  if (br_shown || hal_millis() - br_start < 1400) return;
  br_shown = true;
  int n = players_pick_count();
  if (n < 1) return;
  int order[PLAYER_MAX];
  float dev[PLAYER_MAX];
  for (int i = 0; i < n; i++) {
    order[i] = i;
    dev[i] = fabsf(b_poured[i] - b_goal);
  }
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && dev[order[j]] < dev[order[j - 1]]; j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }
  char b[48], w[16];
  lv_label_set_text(br_win, bname(order[0]));
  for (int k = 0; k < n; k++) {
    int i = order[k];
    lv_obj_t *row = ui_box(br_list);
    lv_obj_set_size(row, 250, 30);
    ui_fmt_1dec(w, sizeof(w), b_poured[i]);
    snprintf(b, sizeof(b), "%d. %.10s · %s g", k + 1, bname(i), w);
    lv_obj_t *l = ui_label(row, b, &font_sg_14, k == 0 ? C_ACCENT : C_TEXT2);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    ui_fmt_1dec(w, sizeof(w), dev[i]);
    snprintf(b, sizeof(b), T("%s g daneben"), w);
    lv_obj_t *r = ui_label(row, b, &font_sg_14, k == 0 ? C_ACCENT : C_MUTED);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
    score_add("Blindgiessen", bname(i), dev[i], "g");
  }
  char note[48];
  snprintf(note, sizeof(note), T("Blindgießen: Sieger %s"), bname(order[0]));
  log_add(b_poured[order[0]], note);
  sound_play(SND_DONE);
}

static lv_obj_t *page_blind_result_create() {
  lv_obj_t *s = ui_screen_create();
  char b[32];
  snprintf(b, sizeof(b), T("Ziel %d g"), b_goal);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);
  br_win = ui_label(s, "…", &font_sg_34, C_ACCENT);
  lv_obj_align(br_win, LV_ALIGN_CENTER, 0, -96);

  br_list = ui_box(s);
  lv_obj_set_size(br_list, 250, 130);
  lv_obj_align(br_list, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_flex_flow(br_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(br_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(br_list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(br_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(br_list, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 120);
  lv_obj_t *again = ui_btn(row, "Nochmal", BTN_PRIMARY);
  lv_obj_add_event_cb(again, [](lv_event_t *e) { ui_switch_page(page_blind_create()); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *home = ui_btn(row, "Zu Wiegen", BTN_NORMAL);
  lv_obj_add_event_cb(home, [](lv_event_t *e) { ui_go_home(); }, LV_EVENT_CLICKED, NULL);

  sound_play(SND_DRUM);
  br_start = hal_millis();
  br_shown = false;
  ui_page_timer(s, br_timer_cb, 100);
  return s;
}
