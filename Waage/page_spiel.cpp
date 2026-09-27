// ============================================================
//  Modi -> Spiel: Schätzspiel (siehe Design-Sheet)
//   Einrichten -> pro Runde: Gegenstand verdeckt auflegen,
//   jeder Spieler tippt, Auflösung mit Trommelwirbel
//   -> nach der letzten Runde Bestenliste (auch auf der SD)
//  Gewertet wird die Summe der Abweichungen: weniger ist besser.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "storage.h"
#include "data.h"
#include "sound.h"
#include "hal.h"
#include "ui_text.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_PLAYERS 8
#define EMPTY_G 3.0f
#define MIN_ITEM_G 5.0f

// Mitspieler werden einmal unter "Spieler" gewählt (gemeinsame Liste in data.h)
#define g_players players_pick_count()

// Name des n-ten Mitspielers dieser Runde
static const char *pick_name(int nth) {
  return player_name(players_pick_index(nth));
}
static int g_rounds = 5;
static int g_round = 0;          // 0-basiert
static int g_player = 0;
static int g_guess[MAX_PLAYERS] = { 500, 500, 500, 500, 500, 500, 500, 500 };
static float g_total[MAX_PLAYERS];
static float g_weight = 0;       // Gewicht der laufenden Runde
static int g_step = 10;

static lv_obj_t *page_round_create();
static lv_obj_t *page_tip_create();
static lv_obj_t *page_reveal_create();
static lv_obj_t *page_board_create();

static lv_obj_t *step_row(lv_obj_t *s, lv_coord_t y, lv_obj_t **btns, const int *vals, int n, int cur,
                          lv_event_cb_t cb) {
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
  for (int i = 0; i < n; i++) {
    char b[8];
    snprintf(b, sizeof(b), "%d", vals[i]);
    btns[i] = ui_btn(row, b, BTN_NORMAL);
    lv_obj_set_height(btns[i], 42);
    lv_obj_set_style_pad_hor(btns[i], 14, 0);
    lv_obj_add_event_cb(btns[i], cb, LV_EVENT_CLICKED, (void *)(intptr_t)vals[i]);
  }
  return row;
}

static void mark(lv_obj_t **btns, const int *vals, int n, int cur) {
  for (int i = 0; i < n; i++) {
    bool on = vals[i] == cur;
    lv_obj_set_style_bg_color(btns[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btns[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

// ------------------------------------------------------------
//  Einrichten
// ------------------------------------------------------------
static lv_obj_t *su_players, *su_rounds[3];
static const int ROUNDS[3] = { 3, 5, 10 };

static void su_show() {
  char b[96];
  b[0] = 0;
  for (int i = 0; i < g_players && i < 6; i++) {
    strncat(b, i ? ", " : "", sizeof(b) - strlen(b) - 1);
    strncat(b, pick_name(i), sizeof(b) - strlen(b) - 1);
  }
  if (!b[0]) snprintf(b, sizeof(b), T("niemand gewählt"));
  lv_label_set_text(su_players, b);
  mark(su_rounds, ROUNDS, 3, g_rounds);
}

static void su_rounds_cb(lv_event_t *e) {
  g_rounds = (int)(intptr_t)lv_event_get_user_data(e);
  su_show();
}

static void su_go_cb(lv_event_t *e) {
  g_round = 0;
  memset(g_total, 0, sizeof(g_total));
  scale_tare();  // Waage ist leer
  ui_switch_page(page_round_create());
}

// ------------------------------------------------------------
//  Namen der Spieler (Buchstabenring)
// ------------------------------------------------------------
static int nm_edit = 0;
static page_create_fn nm_back = NULL;  // wohin "Fertig" führt (Schätz- oder Trinkspiel)
static lv_obj_t *page_names_create();

static void nm_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(g_player_names[nm_edit], text, sizeof(g_player_names[0]) - 1);
    g_player_names[nm_edit][sizeof(g_player_names[0]) - 1] = 0;
    players_save();
  }
  ui_switch_page(page_names_create());
}

static void nm_row_cb(lv_event_t *e) {
  nm_edit = (int)(intptr_t)lv_event_get_user_data(e);
  char t[32];
  static char title[32];
  snprintf(title, sizeof(title), T("Name Spieler %d"), nm_edit + 1);
  const char *cur = g_player_names[nm_edit];
  (void)t;
  ui_switch_page(ui_text_page_create(title, cur, 19, false, NULL, 0, nm_done));
}

static void nm_back_cb(lv_event_t *e) {
  ui_switch_page(nm_back ? nm_back() : page_spiel_create());
}

// Auswahl: Antippen wählt einen Mitspieler aus oder ab
static void nm_pick_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= g_player_count) return;
  g_player_in[i] = !g_player_in[i];
  sound_play(SND_TICK);
  ui_switch_page(page_names_create());
}

// Neuen Spieler anlegen: Name über den Buchstabenring
static void nm_new_done(bool ok, const char *text) {
  if (ok && text[0]) {
    player_add(text);
    int i = player_find(text);
    if (i >= 0) g_player_in[i] = true;
  }
  ui_switch_page(page_names_create());
}

static void nm_new_cb(lv_event_t *e) {
  ui_switch_page(ui_text_page_create("Neuer Spieler", "", 19, false, NULL, 0, nm_new_done));
}

lv_obj_t *page_players_create(page_create_fn back) {
  nm_back = back;
  return page_names_create();
}

static lv_obj_t *page_names_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Mitspieler wählen", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_t *h = ui_label(s, "Antippen = dabei · Halten = umbenennen", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 70);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 280, 186);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  for (int i = 0; i < g_player_count; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 280, 46);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, nm_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(row, nm_row_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    lv_obj_t *n = ui_label(row, g_player_in[i] ? "•" : "", &font_sg_24, C_ACCENT);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_t *l = ui_label(row, player_name(i), &font_sg_18, g_player_in[i] ? C_TEXT : C_FAINT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 34, 0);
    lv_obj_t *c = ui_label(row, g_player_in[i] ? "dabei" : "", &font_sg_14, C_ACCENT);
    lv_obj_align(c, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 140);
  lv_obj_t *nw = ui_btn(row, "+ Spieler", BTN_NORMAL);
  lv_obj_add_event_cb(nw, nm_new_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *b = ui_btn(row, "Fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(b, nm_back_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

static void su_names_cb(lv_event_t *e) {
  ui_switch_page(page_players_create(page_spiel_create));
}

lv_obj_t *page_spiel_create() {
  static bool loaded = false;
  if (!loaded) {  // Namen einmal von der SD holen
    players_load();
    loaded = true;
  }
  players_pick_default();
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Schätzspiel", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t *l1 = ui_label(s, "Mitspieler", &font_sg_18, C_MUTED);
  lv_obj_align(l1, LV_ALIGN_CENTER, 0, -100);
  su_players = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_set_width(su_players, 280);
  lv_label_set_long_mode(su_players, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(su_players, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(su_players, LV_ALIGN_CENTER, 0, -72);
  lv_obj_t *pick = ui_btn(s, "Auswählen ›", BTN_NORMAL);
  lv_obj_align(pick, LV_ALIGN_CENTER, 0, -34);
  lv_obj_add_event_cb(pick, su_names_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *l2 = ui_label(s, "Runden", &font_sg_18, C_MUTED);
  lv_obj_align(l2, LV_ALIGN_CENTER, 0, 6);
  step_row(s, 46, su_rounds, ROUNDS, 3, g_rounds, su_rounds_cb);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 110);
  lv_obj_t *nb = ui_btn(row, "Spieler ›", BTN_NORMAL);
  lv_obj_add_event_cb(nb, su_names_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *go = ui_btn(row, "Los", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 34, 0);
  lv_obj_add_event_cb(go, su_go_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Waage vorher leeren", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 156);

  su_show();
  return s;
}

// ------------------------------------------------------------
//  Rundenstart: Gegenstand verdeckt auflegen
// ------------------------------------------------------------
static lv_obj_t *rd_info, *rd_btn;
static bool rd_need_empty;

static void rd_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  bool ready = false;
  if (rd_need_empty) {
    if (fabsf(g) < EMPTY_G) rd_need_empty = false;
    ui_label_update(rd_info, "Gegenstand abnehmen");
  } else if (g < MIN_ITEM_G) {
    ui_label_update(rd_info, "Gegenstand verdeckt auflegen");
  } else if (!scale_stable()) {
    ui_label_update(rd_info, "misst …");
  } else {
    ui_label_update(rd_info, "Bereit – nicht mehr berühren");
    ready = true;
  }
  if (ready) lv_obj_clear_state(rd_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(rd_btn, LV_STATE_DISABLED);
}

static void rd_go_cb(lv_event_t *e) {
  if (!scale_stable() || scale_net() < MIN_ITEM_G) return;
  g_player = 0;
  ui_switch_page(page_tip_create());
}

static lv_obj_t *page_round_create() {
  lv_obj_t *s = ui_screen_create();
  char b[32];
  snprintf(b, sizeof(b), T("Runde %d / %d"), g_round + 1, g_rounds);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *q = ui_label(s, "? ? ?", &font_sg_80, C_FAINT);
  lv_obj_align(q, LV_ALIGN_CENTER, 0, -34);

  rd_info = ui_label(s, "", &font_sg_18, C_TEXT);
  lv_obj_align(rd_info, LV_ALIGN_CENTER, 0, 40);

  rd_btn = ui_btn(s, "Tippen", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(rd_btn, 36, 0);
  lv_obj_align(rd_btn, LV_ALIGN_CENTER, 0, 104);
  lv_obj_add_event_cb(rd_btn, rd_go_cb, LV_EVENT_CLICKED, NULL);

  rd_need_empty = g_round > 0;  // ab Runde 2 erst den alten Gegenstand abnehmen
  ui_page_timer(s, rd_timer_cb, 100);
  rd_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Tipp eines Spielers
// ------------------------------------------------------------
static lv_obj_t *tp_value, *tp_steps[3];
static const int STEPS[3] = { 1, 10, 100 };

static void tp_show() {
  char b[16];
  snprintf(b, sizeof(b), "%d g", g_guess[g_player]);
  lv_label_set_text(tp_value, b);
  mark(tp_steps, STEPS, 3, g_step);
}

static void tp_adj_cb(lv_event_t *e) {
  int up = (int)(intptr_t)lv_event_get_user_data(e);
  g_guess[g_player] += up ? g_step : -g_step;
  if (g_guess[g_player] < 0) g_guess[g_player] = 0;
  if (g_guess[g_player] > 9999) g_guess[g_player] = 9999;
  tp_show();
}

static void tp_step_cb(lv_event_t *e) {
  g_step = (int)(intptr_t)lv_event_get_user_data(e);
  tp_show();
}

static void tp_ok_cb(lv_event_t *e) {
  sound_play(SND_TICK);
  if (g_player + 1 < g_players) {
    g_player++;
    ui_switch_page(page_tip_create());
    return;
  }
  g_weight = scale_net();  // Gegenstand liegt noch unberührt auf
  ui_switch_page(page_reveal_create());
}

static lv_obj_t *page_tip_create() {
  lv_obj_t *s = ui_screen_create();
  char b[40];
  snprintf(b, sizeof(b), T("Schätzspiel · Runde %d"), g_round + 1);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *q = ui_label(s, "? ? ?", &font_sg_34, C_FAINT);
  lv_obj_align(q, LV_ALIGN_CENTER, 0, -104);

  snprintf(b, sizeof(b), "%s", pick_name(g_player));
  lv_obj_t *h = ui_label(s, b, &font_sg_24, C_TEXT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -60);

  tp_value = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(tp_value, LV_ALIGN_CENTER, 0, -8);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_align(m, LV_ALIGN_CENTER, -120, -8);
  lv_obj_add_event_cb(m, tp_adj_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_align(p, LV_ALIGN_CENTER, 120, -8);
  lv_obj_add_event_cb(p, tp_adj_cb, LV_EVENT_CLICKED, (void *)1);

  step_row(s, 50, tp_steps, STEPS, 3, g_step, tp_step_cb);

  lv_obj_t *ok = ui_btn(s, "Tipp abgeben", BTN_PRIMARY);
  lv_obj_align(ok, LV_ALIGN_CENTER, 0, 112);
  lv_obj_add_event_cb(ok, tp_ok_cb, LV_EVENT_CLICKED, NULL);

  tp_show();
  return s;
}

// ------------------------------------------------------------
//  Auflösung
// ------------------------------------------------------------
static lv_obj_t *rv_weight, *rv_list, *rv_btn;
static uint32_t rv_start;
static bool rv_shown;

static void rv_timer_cb(lv_timer_t *t) {
  if (rv_shown || hal_millis() - rv_start < 1300) return;  // Trommelwirbel abwarten
  rv_shown = true;
  char b[48];
  snprintf(b, sizeof(b), "%d", (int)lroundf(g_weight));
  lv_label_set_text(rv_weight, b);
  lv_obj_set_style_text_color(rv_weight, C_ACCENT, 0);

  // Spieler nach Abweichung dieser Runde sortieren
  int order[MAX_PLAYERS];
  float dev[MAX_PLAYERS];
  for (int i = 0; i < g_players; i++) {
    order[i] = i;
    dev[i] = fabsf(g_guess[i] - g_weight);
  }
  for (int i = 1; i < g_players; i++)
    for (int j = i; j > 0 && dev[order[j]] < dev[order[j - 1]]; j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }
  for (int k = 0; k < g_players; k++) {
    int i = order[k];
    lv_obj_t *row = ui_box(rv_list);
    lv_obj_set_size(row, 250, 30);
    snprintf(b, sizeof(b), "%.10s · %d g", pick_name(i), g_guess[i]);
    lv_obj_t *l = ui_label(row, b, &font_sg_14, k == 0 ? C_ACCENT : C_TEXT2);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    snprintf(b, sizeof(b), T("%d g daneben"), (int)lroundf(dev[i]));
    lv_obj_t *r = ui_label(row, b, &font_sg_14, k == 0 ? C_ACCENT : C_MUTED);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  lv_obj_clear_flag(rv_btn, LV_OBJ_FLAG_HIDDEN);
}

static void rv_next_cb(lv_event_t *e) {
  for (int i = 0; i < g_players; i++) g_total[i] += fabsf(g_guess[i] - g_weight);
  g_round++;
  if (g_round >= g_rounds) ui_switch_page(page_board_create());
  else ui_switch_page(page_round_create());
}

static lv_obj_t *page_reveal_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Auflösung", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 46);

  rv_weight = ui_label(s, "? ? ?", &font_sg_80, C_FAINT);
  lv_obj_align(rv_weight, LV_ALIGN_CENTER, 0, -86);
  lv_obj_t *g = ui_label(s, "g", &font_sg_18, C_MUTED);
  lv_obj_align(g, LV_ALIGN_CENTER, 0, -34);

  rv_list = ui_box(s);
  lv_obj_set_size(rv_list, 250, 124);
  lv_obj_align(rv_list, LV_ALIGN_CENTER, 0, 34);
  lv_obj_set_flex_flow(rv_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(rv_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(rv_list, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(rv_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(rv_list, LV_SCROLLBAR_MODE_OFF);

  rv_btn = ui_btn(s, g_round + 1 >= g_rounds ? "Endstand" : "Nächste Runde", BTN_PRIMARY);
  lv_obj_align(rv_btn, LV_ALIGN_CENTER, 0, 130);
  lv_obj_add_event_cb(rv_btn, rv_next_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(rv_btn, LV_OBJ_FLAG_HIDDEN);

  rv_start = hal_millis();
  rv_shown = false;
  sound_play(SND_DRUM);
  ui_page_timer(s, rv_timer_cb, 100);
  return s;
}

// ------------------------------------------------------------
//  Bestenliste
// ------------------------------------------------------------
static void bd_new_cb(lv_event_t *e) {
  ui_switch_page(page_spiel_create());
}

static lv_obj_t *page_board_create() {
  int order[MAX_PLAYERS];
  for (int i = 0; i < g_players; i++) order[i] = i;
  for (int i = 1; i < g_players; i++)
    for (int j = i; j > 0 && g_total[order[j]] < g_total[order[j - 1]]; j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }

  // Ergebnis auf die SD schreiben
  char line[160], day[11];
  log_today(day);
  int h, mi;
  hal_time(&h, &mi);
  int n = snprintf(line, sizeof(line), "%s %02d:%02d;%d Runden", day[0] ? day : "ohne Datum", h, mi, g_rounds);
  for (int k = 0; k < g_players && n < (int)sizeof(line) - 24; k++)
    n += snprintf(line + n, sizeof(line) - n, ";%s:%d", pick_name(order[k]), (int)lroundf(g_total[order[k]]));
  storage_append(DIR_GAME "/spiele.txt", line);
  char note[48];
  snprintf(note, sizeof(note), T("Schätzspiel: Sieger %s"), pick_name(order[0]));
  for (int k = 0; k < g_players; k++) score_add("Schaetzspiel", pick_name(order[k]), g_total[order[k]], "g");
  log_add(g_total[order[0]], note);
  sound_play(SND_DONE);

  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Bestenliste", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 260, 170);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -14);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  for (int k = 0; k < g_players; k++) {
    int i = order[k];
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 260, 40);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    char b[32];
    snprintf(b, sizeof(b), "%d · %.12s", k + 1, pick_name(i));
    lv_obj_t *l = ui_label(row, b, &font_sg_18, k == 0 ? C_ACCENT : C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    snprintf(b, sizeof(b), "%d g", (int)lroundf(g_total[i]));
    lv_obj_t *r = ui_label(row, b, &font_sg_18, k == 0 ? C_ACCENT : C_MUTED);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  lv_obj_t *h2 = ui_label(s, "Summe der Abweichungen", &font_sg_14, C_FAINT);
  lv_obj_align(h2, LV_ALIGN_CENTER, 0, 84);

  lv_obj_t *b = ui_btn(s, "Neues Spiel", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 130);
  lv_obj_add_event_cb(b, bd_new_cb, LV_EVENT_CLICKED, NULL);
  return s;
}
