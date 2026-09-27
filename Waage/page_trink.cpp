// ============================================================
//  Modi -> Trinkspiel (Prinzip wie SipMaster: die Waage misst die
//  Schluckgröße, gewonnen wird durch gutes Einschätzen)
//
//  Ablauf pro Zug:  Getränk abstellen -> trinken -> zurückstellen
//  Schluck = Gewicht vorher - Gewicht nachher (1 g ≈ 1 ml)
//
//  Varianten:
//   Zielschluck: jede Runde ein neues Ziel, Summe der Abweichungen zählt
//   KO:          wer weiter daneben liegt als die Toleranz, ist raus;
//                die Toleranz wird jede Runde kleiner
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "storage.h"
#include "data.h"

// Mitspieler dieser Runde (Auswahl unter "Spieler", gemeinsame Liste)
#define T_COUNT players_pick_count()
static const char *pick_name(int nth) {
  return player_name(players_pick_index(nth));
}
#include "sound.h"
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIN_DRINK_G 20.0f   // leichter = kein Getränk
#define LIFT_G 10.0f        // darunter gilt das Getränk als angehoben
#define NOTHING_ML 1.0f     // kleiner = nichts getrunken (doppelt abgestellt)
#define KO_TOL_START 15
#define KO_TOL_STEP 3
#define KO_TOL_MIN 3

static int t_mode = 0;       // 0 Zielschluck, 1 KO
static int t_rounds = 5;
static int t_round = 0;
static int t_player = 0;
static int t_target = 40;
static int t_tol = KO_TOL_START;
static float t_sip[PLAYER_MAX];     // Schluck dieser Runde
static float t_dev_sum[PLAYER_MAX]; // Summe der Abweichungen
static float t_drunk[PLAYER_MAX];   // insgesamt getrunken (Statistik)
static bool t_out[PLAYER_MAX];      // KO: ausgeschieden
static int t_out_round[PLAYER_MAX]; // KO: in welcher Runde raus

static lv_obj_t *page_turn_create();
static lv_obj_t *page_round_end_create();
static lv_obj_t *page_board_create();

static int alive_count() {
  int n = 0;
  for (int i = 0; i < T_COUNT; i++)
    if (!t_out[i]) n++;
  return n;
}

static void new_target() {
  t_target = 10 + (rand() % 15) * 5;  // 10 ... 80 ml in 5er-Schritten
}

static int next_player(int from) {
  for (int i = from; i < T_COUNT; i++)
    if (!t_out[i]) return i;
  return -1;
}

// ------------------------------------------------------------
//  Einrichten
// ------------------------------------------------------------
static lv_obj_t *su_mode[2], *su_rounds[3], *su_rlabel, *su_rrow, *su_info;
static const int ROUNDS[3] = { 3, 5, 10 };

static void su_show() {
  for (int i = 0; i < 2; i++) {
    lv_obj_set_style_bg_color(su_mode[i], i == t_mode ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(su_mode[i], 0), i == t_mode ? C_BG : C_TEXT2, 0);
  }
  for (int i = 0; i < 3; i++) {
    bool on = ROUNDS[i] == t_rounds;
    lv_obj_set_style_bg_color(su_rounds[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(su_rounds[i], 0), on ? C_BG : C_TEXT2, 0);
  }
  if (t_mode == 0) {
    lv_obj_clear_flag(su_rrow, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(su_rlabel, "Runden");
  } else {
    lv_obj_add_flag(su_rrow, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(su_rlabel, "bis einer übrig ist");
  }
  char b[40];
  b[0] = 0;
  for (int i = 0; i < T_COUNT && i < 6; i++) {
    strncat(b, i ? ", " : "", sizeof(b) - strlen(b) - 1);
    strncat(b, pick_name(i), sizeof(b) - strlen(b) - 1);
  }
  if (!b[0]) snprintf(b, sizeof(b), T("niemand gewählt"));
  lv_label_set_text(su_info, b);
}

static void su_mode_cb(lv_event_t *e) {
  t_mode = (int)(intptr_t)lv_event_get_user_data(e);
  su_show();
}

static void su_rounds_cb(lv_event_t *e) {
  t_rounds = (int)(intptr_t)lv_event_get_user_data(e);
  su_show();
}

static void su_names_cb(lv_event_t *e) {
  ui_switch_page(page_players_create(page_trink_create));
}

static void su_go_cb(lv_event_t *e) {
  srand(hal_millis());
  t_round = 0;
  t_tol = KO_TOL_START;
  memset(t_dev_sum, 0, sizeof(t_dev_sum));
  memset(t_drunk, 0, sizeof(t_drunk));
  memset(t_out, 0, sizeof(t_out));
  scale_tare();  // Waage ist leer
  new_target();
  t_player = next_player(0);
  ui_switch_page(page_turn_create());
}

static lv_obj_t *seg_btn(lv_obj_t *row, const char *txt, lv_event_cb_t cb, int val) {
  lv_obj_t *b = ui_btn(row, txt, BTN_NORMAL);
  lv_obj_set_height(b, 42);
  lv_obj_set_style_pad_hor(b, 14, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)val);
  return b;
}

static lv_obj_t *hrow(lv_obj_t *s, lv_coord_t y) {
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
  return row;
}

lv_obj_t *page_trink_create() {
  players_load();
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Trinkspiel", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 46);

  lv_obj_t *r1 = hrow(s, -88);
  su_mode[0] = seg_btn(r1, "Zielschluck", su_mode_cb, 0);
  su_mode[1] = seg_btn(r1, "KO", su_mode_cb, 1);

  su_rlabel = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(su_rlabel, LV_ALIGN_CENTER, 0, -44);
  su_rrow = hrow(s, -12);
  for (int i = 0; i < 3; i++) {
    char b[6];
    snprintf(b, sizeof(b), "%d", ROUNDS[i]);
    su_rounds[i] = seg_btn(su_rrow, b, su_rounds_cb, ROUNDS[i]);
  }

  lv_obj_t *r3 = hrow(s, 50);
  su_info = ui_label(r3, "", &font_sg_18, C_TEXT);
  lv_obj_t *nb = ui_btn(r3, "Spieler ›", BTN_NORMAL);
  lv_obj_set_height(nb, 42);
  lv_obj_add_event_cb(nb, su_names_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_flex_align(r3, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *go = ui_btn(s, "Los", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 40, 0);
  lv_obj_align(go, LV_ALIGN_CENTER, 0, 112);
  lv_obj_add_event_cb(go, su_go_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Bitte verantwortungsvoll trinken", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 152);

  su_show();
  return s;
}

// ------------------------------------------------------------
//  Ein Zug: abstellen -> trinken -> zurückstellen
// ------------------------------------------------------------
typedef enum { S_PLACE, S_DRINK, S_BACK, S_REVEAL, S_DONE } turn_t;
static turn_t tn_state;
static float tn_before;
static uint32_t tn_reveal_at;
static lv_obj_t *tn_status, *tn_big, *tn_unit, *tn_sub, *tn_btn, *tn_toast;

static void tn_next_cb(lv_event_t *e) {
  int nx = next_player(t_player + 1);
  if (nx >= 0) {
    t_player = nx;
    ui_switch_page(page_turn_create());
  } else {
    ui_switch_page(page_round_end_create());
  }
}

static void tn_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  bool st = scale_stable();
  char b[48];
  switch (tn_state) {
    case S_PLACE:
      ui_label_update(tn_status, "Getränk abstellen");
      if (st && g >= MIN_DRINK_G) {
        tn_before = g;
        tn_state = S_DRINK;
        sound_play(SND_TICK);
      }
      break;
    case S_DRINK:
      ui_label_update(tn_status, "Jetzt trinken!");
      if (g < LIFT_G) tn_state = S_BACK;
      break;
    case S_BACK:
      ui_label_update(tn_status, "Zurückstellen");
      if (st && g >= LIFT_G) {
        float sip = tn_before - g;
        if (sip < NOTHING_ML) {  // doppelt abgestellt, nichts getrunken
          ui_toast_show(tn_toast, "Nichts getrunken? Nochmal", C_WARN);
          tn_before = g;
          tn_state = S_DRINK;
          break;
        }
        t_sip[t_player] = sip;
        t_drunk[t_player] += sip;
        tn_state = S_REVEAL;
        tn_reveal_at = hal_millis() + 1300;
        ui_label_update(tn_status, "Und …");
        sound_play(SND_DRUM);
      }
      break;
    case S_REVEAL:
      if (hal_millis() < tn_reveal_at) break;
      {
        float sip = t_sip[t_player];
        float dev = fabsf(sip - t_target);
        snprintf(b, sizeof(b), "%d", (int)lroundf(sip));
        lv_label_set_text(tn_big, b);
        lv_obj_clear_flag(tn_unit, LV_OBJ_FLAG_HIDDEN);
        bool hit = dev <= 2.0f;
        lv_obj_set_style_text_color(tn_big, hit ? C_ACCENT : C_TEXT, 0);
        if (t_mode == 1) {
          bool out = dev > t_tol;
          snprintf(b, sizeof(b), T("%d ml daneben · %s"), (int)lroundf(dev), out ? T("raus!") : T("weiter"));
          lv_obj_set_style_text_color(tn_sub, out ? C_DANGER : C_ACCENT, 0);
        } else {
          snprintf(b, sizeof(b), hit ? T("Volltreffer! %d ml daneben") : T("%d ml daneben"), (int)lroundf(dev));
          lv_obj_set_style_text_color(tn_sub, hit ? C_ACCENT : C_MUTED, 0);
        }
        ui_label_update(tn_sub, b);
        ui_label_update(tn_status, "Ergebnis");
        lv_obj_clear_flag(tn_btn, LV_OBJ_FLAG_HIDDEN);
        tn_state = S_DONE;
      }
      break;
    default:
      break;
  }
}

static lv_obj_t *page_turn_create() {
  lv_obj_t *s = ui_screen_create();
  char b[48];
  if (t_mode == 1) snprintf(b, sizeof(b), T("Runde %d · Ziel %d ml · ±%d"), t_round + 1, t_target, t_tol);
  else snprintf(b, sizeof(b), T("Runde %d / %d · Ziel %d ml"), t_round + 1, t_rounds, t_target);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *n = ui_label(s, pick_name(t_player), &font_sg_34, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -104);

  tn_status = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(tn_status, LV_ALIGN_CENTER, 0, -62);

  snprintf(b, sizeof(b), "%d", t_target);
  tn_big = ui_label(s, b, &font_sg_80, C_FAINT);  // zuerst das Ziel, nach dem Zug der Schluck
  lv_obj_align(tn_big, LV_ALIGN_CENTER, 0, 4);
  tn_unit = ui_label(s, "ml", &font_sg_18, C_MUTED);
  lv_obj_align(tn_unit, LV_ALIGN_CENTER, 0, 60);

  tn_sub = ui_label(s, "Ziel", &font_sg_18, C_MUTED);
  lv_obj_align(tn_sub, LV_ALIGN_CENTER, 0, 88);

  tn_btn = ui_btn(s, "Weiter", BTN_PRIMARY);
  lv_obj_align(tn_btn, LV_ALIGN_CENTER, 0, 138);
  lv_obj_add_event_cb(tn_btn, tn_next_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(tn_btn, LV_OBJ_FLAG_HIDDEN);

  tn_toast = ui_toast_create(s);
  lv_obj_align(tn_toast, LV_ALIGN_CENTER, 0, 138);

  tn_state = S_PLACE;
  ui_page_timer(s, tn_timer_cb, 100);
  tn_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Rundenende
// ------------------------------------------------------------
static void re_next_cb(lv_event_t *e) {
  t_round++;
  bool end = (t_mode == 0) ? t_round >= t_rounds : alive_count() <= 1;
  if (end) {
    ui_switch_page(page_board_create());
    return;
  }
  if (t_mode == 1) {
    t_tol -= KO_TOL_STEP;
    if (t_tol < KO_TOL_MIN) t_tol = KO_TOL_MIN;
  }
  new_target();
  t_player = next_player(0);
  ui_switch_page(page_turn_create());
}

static lv_obj_t *page_round_end_create() {
  // Wertung der Runde
  int alive_before = alive_count();
  int would_stay = 0;
  for (int i = 0; i < T_COUNT; i++) {
    if (t_out[i]) continue;
    float dev = fabsf(t_sip[i] - t_target);
    t_dev_sum[i] += dev;
    if (dev <= t_tol) would_stay++;
  }
  if (t_mode == 1 && would_stay > 0) {  // raus nur, wenn nicht alle raus wären
    for (int i = 0; i < T_COUNT; i++)
      if (!t_out[i] && fabsf(t_sip[i] - t_target) > t_tol) {
        t_out[i] = true;
        t_out_round[i] = t_round;
      }
  }

  lv_obj_t *s = ui_screen_create();
  char b[48];
  snprintf(b, sizeof(b), T("Runde %d · Ziel %d ml"), t_round + 1, t_target);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 270, 200);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -6);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  for (int i = 0; i < T_COUNT; i++) {
    bool played = !t_out[i] || t_out_round[i] == t_round;
    if (!played) continue;
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 270, 40);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    bool out_now = t_out[i] && t_out_round[i] == t_round;
    lv_obj_t *l = ui_label(row, pick_name(i), &font_sg_18, out_now ? C_DANGER : C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    snprintf(b, sizeof(b), T("%d ml · %d daneben"), (int)lroundf(t_sip[i]), (int)lroundf(fabsf(t_sip[i] - t_target)));
    lv_obj_t *r = ui_label(row, b, &font_sg_14, out_now ? C_DANGER : C_MUTED);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  (void)alive_before;

  bool end = (t_mode == 0) ? t_round + 1 >= t_rounds : alive_count() <= 1;
  lv_obj_t *nb = ui_btn(s, end ? "Endstand" : "Nächste Runde", BTN_PRIMARY);
  lv_obj_align(nb, LV_ALIGN_CENTER, 0, 132);
  lv_obj_add_event_cb(nb, re_next_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

// ------------------------------------------------------------
//  Endstand mit Statistik
// ------------------------------------------------------------
static void bd_new_cb(lv_event_t *e) {
  ui_switch_page(page_trink_create());
}

static lv_obj_t *page_board_create() {
  int order[PLAYER_MAX];
  int n = T_COUNT;
  for (int i = 0; i < n; i++) order[i] = i;
  // KO: länger im Spiel = besser; Zielschluck: kleinere Summe = besser
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0; j--) {
      int a = order[j], c = order[j - 1];
      bool better;
      if (t_mode == 1) {
        int ra = t_out[a] ? t_out_round[a] : 999, rc = t_out[c] ? t_out_round[c] : 999;
        better = ra > rc || (ra == rc && t_dev_sum[a] < t_dev_sum[c]);
      } else {
        better = t_dev_sum[a] < t_dev_sum[c];
      }
      if (!better) break;
      order[j] = c;
      order[j - 1] = a;
    }

  // Protokoll und Spieldatei
  char line[200], day[11], note[48];
  log_today(day);
  int h, mi;
  hal_time(&h, &mi);
  int k = snprintf(line, sizeof(line), "%s %02d:%02d;Trinkspiel %s", day[0] ? day : "ohne Datum", h, mi,
                   t_mode ? "KO" : "Zielschluck");
  for (int i = 0; i < n && k < (int)sizeof(line) - 32; i++)
    k += snprintf(line + k, sizeof(line) - k, ";%s:%d ml getrunken", pick_name(order[i]),
                  (int)lroundf(t_drunk[order[i]]));
  storage_append(DIR_GAME "/spiele.txt", line);
  snprintf(note, sizeof(note), T("Trinkspiel: Sieger %s"), pick_name(order[0]));
  for (int i = 0; i < T_COUNT; i++) score_add("Trinkspiel", pick_name(i), t_dev_sum[i], "ml");
  log_add(t_dev_sum[order[0]], note);
  sound_play(SND_DONE);

  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Sieg!", &font_sg_24, C_ACCENT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_t *w = ui_label(s, pick_name(order[0]), &font_sg_34, C_TEXT);
  lv_obj_align(w, LV_ALIGN_CENTER, 0, -104);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 270, 170);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  for (int r = 0; r < n; r++) {
    int i = order[r];
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 270, 40);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    char b[40];
    snprintf(b, sizeof(b), "%d · %.12s", r + 1, pick_name(i));
    lv_obj_t *l = ui_label(row, b, &font_sg_18, r == 0 ? C_ACCENT : C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    if (t_mode == 1) snprintf(b, sizeof(b), T("%s · %d ml"), t_out[i] ? T("raus") : T("Sieg"), (int)lroundf(t_drunk[i]));
    else snprintf(b, sizeof(b), T("±%d · %d ml"), (int)lroundf(t_dev_sum[i]), (int)lroundf(t_drunk[i]));
    lv_obj_t *v = ui_label(row, b, &font_sg_14, C_MUTED);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  lv_obj_t *h2 = ui_label(s, t_mode ? "Rang · getrunken" : "Summe daneben · getrunken", &font_sg_14, C_FAINT);
  lv_obj_align(h2, LV_ALIGN_CENTER, 0, 104);

  lv_obj_t *b = ui_btn(s, "Neues Spiel", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 144);
  lv_obj_add_event_cb(b, bd_new_cb, LV_EVENT_CLICKED, NULL);
  return s;
}
