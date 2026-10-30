// ============================================================
//  Modi -> Spiele -> Ex: Getränk so schnell wie möglich leeren
//
//  Ablauf pro Spieler:  Getränk abstellen -> anheben startet die Zeit
//                       -> zurückstellen stoppt sie
//  Gestoppt wird beim Aufsetzen (ungefilterter Wert), die Menge wird
//  danach gemessen, sobald das Glas ruhig steht (1 g ≈ 1 ml).
//  Wird das Glas vor dem Ruhigwerden noch einmal angehoben, läuft die
//  Zeit weiter (nur kurz abgesetzt).
//
//  Bestenliste: Spiel "Ex", Wert = Zeit in s (kleiner ist besser),
//  zweiter Wert = getrunkene ml. Erst ab EX_MIN_ML, sonst gewinnt,
//  wer einen Schluck nimmt.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "storage.h"
#include "data.h"
#include "sound.h"
#include "hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define EX_GAME "Ex"
#define MIN_DRINK_G 20.0f   // leichter = kein Getränk
#define LIFT_G 10.0f        // darunter gilt das Glas als angehoben
#define NOTHING_ML 3.0f     // weniger = nichts getrunken
#define EX_MIN_ML 50        // ab dieser Menge zählt es für die Bestenliste
#define SETTLE_MS 600       // so lange nach dem Aufsetzen mindestens warten
#define MAX_S 599.9f

static int x_player = 0;              // n-ter Mitspieler
static float x_time[PLAYER_MAX];      // s
static float x_ml[PLAYER_MAX];
static score_t x_rec;                 // Rekord vor diesem Spiel
static bool x_has_rec = false;

static lv_obj_t *page_ex_turn_create();
static lv_obj_t *page_ex_board_create();

static const char *xname(int nth) {
  return player_name(players_pick_index(nth));
}

// "4,3 s · 330 ml"
static void fmt_result(char *b, int len, float s, float ml) {
  char t[12];
  ui_fmt_1dec(t, sizeof(t), s);
  snprintf(b, len, "%s s · %d ml", t, (int)lroundf(ml));
}

// ------------------------------------------------------------
//  Einrichten
// ------------------------------------------------------------
static lv_obj_t *xs_players;

static void xs_names_cb(lv_event_t *e) {
  ui_switch_page(page_players_create(page_ex_create));
}

static void xs_go_cb(lv_event_t *e) {
  if (players_pick_count() < 1) {
    sound_play(SND_WARN);
    return;
  }
  x_player = 0;
  memset(x_time, 0, sizeof(x_time));
  memset(x_ml, 0, sizeof(x_ml));
  x_has_rec = score_best(EX_GAME, &x_rec);
  scale_tare();  // Waage ist leer
  ui_switch_page(page_ex_turn_create());
}

lv_obj_t *page_ex_create() {
  static bool loaded = false;
  if (!loaded) {
    players_load();
    loaded = true;
  }
  players_pick_default();
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Ex", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  char b[96];
  b[0] = 0;
  for (int i = 0; i < players_pick_count() && i < 6; i++) {
    strncat(b, i ? ", " : "", sizeof(b) - strlen(b) - 1);
    strncat(b, xname(i), sizeof(b) - strlen(b) - 1);
  }
  if (!b[0]) snprintf(b, sizeof(b), "%s", T("niemand gewählt"));
  xs_players = ui_label(s, b, &font_sg_18, C_ACCENT);
  lv_obj_set_width(xs_players, 280);
  lv_label_set_long_mode(xs_players, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(xs_players, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(xs_players, LV_ALIGN_CENTER, 0, -102);

  // Rekord aus der Bestenliste
  lv_obj_t *rl = ui_label(s, "Rekord", &font_sg_14, C_MUTED);
  lv_obj_align(rl, LV_ALIGN_CENTER, 0, -62);
  score_t r;
  if (score_best(EX_GAME, &r)) {
    char v[40];
    fmt_result(v, sizeof(v), r.value, r.extra);
    snprintf(b, sizeof(b), "%.12s · %s", r.name, v);
  } else {
    snprintf(b, sizeof(b), "%s", T("noch keiner"));
  }
  lv_obj_t *rv = ui_label(s, b, &font_sg_18, C_TEXT);
  lv_obj_set_width(rv, 300);
  lv_label_set_long_mode(rv, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(rv, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(rv, LV_ALIGN_CENTER, 0, -36);

  lv_obj_t *h = ui_label(s, "Glas abstellen. Anheben startet die Zeit, Abstellen stoppt sie.", &font_sg_14, C_FAINT);
  lv_obj_set_width(h, 270);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 22);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 96);
  lv_obj_t *nb = ui_btn(row, "Spieler ›", BTN_NORMAL);
  lv_obj_add_event_cb(nb, xs_names_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *go = ui_btn(row, "Los", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 34, 0);
  lv_obj_add_event_cb(go, xs_go_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *w = ui_label(s, "Bitte verantwortungsvoll trinken", &font_sg_14, C_FAINT);
  lv_obj_align(w, LV_ALIGN_CENTER, 0, 148);
  return s;
}

// ------------------------------------------------------------
//  Ein Spieler: abstellen -> anheben (Start) -> abstellen (Stopp)
// ------------------------------------------------------------
typedef enum { X_PLACE, X_READY, X_DRINK, X_SETTLE, X_DONE } xstate_t;
static xstate_t xn_state;
static float xn_before;
static uint32_t xn_up, xn_down;
static lv_obj_t *xn_status, *xn_big, *xn_sub, *xn_btn, *xn_toast;

// schneller als das geglättete Gewicht, damit Start und Stopp nicht nachhinken
static float raw_net() {
  return scale_gross_raw() - scale_get_tare();
}

static void show_time(float s, lv_color_t col) {
  char b[12];
  if (s > MAX_S) s = MAX_S;
  ui_fmt_1dec(b, sizeof(b), s);
  ui_label_update(xn_big, b);
  lv_obj_set_style_text_color(xn_big, col, 0);
}

static void xn_next_cb(lv_event_t *e) {
  if (x_player + 1 < players_pick_count()) {
    x_player++;
    ui_switch_page(page_ex_turn_create());
  } else {
    ui_switch_page(page_ex_board_create());
  }
}

static void xn_finish() {
  float ml = xn_before - scale_net();
  float s = (xn_down - xn_up) / 1000.0f;
  if (ml < NOTHING_ML) {  // nur angehoben und wieder hingestellt
    ui_toast_show(xn_toast, "Nichts getrunken? Nochmal", C_WARN);
    xn_before = scale_net();
    show_time(0, C_FAINT);
    xn_state = X_READY;
    return;
  }
  x_time[x_player] = s;
  x_ml[x_player] = ml;
  bool counts = ml >= EX_MIN_ML;
  show_time(s, counts ? C_ACCENT : C_TEXT2);
  char b[64];
  char t[12];
  snprintf(t, sizeof(t), "%d", (int)lroundf(ml / (s > 0.1f ? s : 0.1f)));
  snprintf(b, sizeof(b), T("%d ml · %s ml/s"), (int)lroundf(ml), t);
  ui_label_update(xn_sub, b);
  if (counts && (!x_has_rec || s < x_rec.value)) {
    ui_label_update(xn_status, "Neuer Rekord!");
    x_has_rec = true;  // gilt ab jetzt für die nächsten Spieler
    strncpy(x_rec.name, xname(x_player), sizeof(x_rec.name) - 1);
    x_rec.value = s;
    x_rec.extra = ml;
    sound_play(SND_DONE);
  } else {
    ui_label_update(xn_status, counts ? "Geschafft" : "Unter 50 ml – zählt nicht");
    if (!counts) lv_obj_set_style_text_color(xn_status, C_WARN, 0);
    sound_play(SND_TICK);
  }
  lv_obj_clear_flag(xn_btn, LV_OBJ_FLAG_HIDDEN);
  xn_state = X_DONE;
}

static void xn_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  float raw = raw_net();
  bool st = scale_stable();
  uint32_t now = hal_millis();
  switch (xn_state) {
    case X_PLACE:
      ui_label_update(xn_status, "Glas abstellen");
      if (st && g >= MIN_DRINK_G) {
        xn_before = g;
        xn_state = X_READY;
        sound_play(SND_TICK);
        char b[40];
        snprintf(b, sizeof(b), T("%d ml im Glas"), (int)lroundf(g));
        ui_label_update(xn_sub, b);
      }
      break;
    case X_READY:
      ui_label_update(xn_status, "Anheben startet die Zeit");
      if (raw < LIFT_G) {
        xn_up = now;
        xn_state = X_DRINK;
      } else if (st && fabsf(g - xn_before) > 2.0f) {
        xn_before = g;  // nachgeschenkt oder ausgetauscht
        char b[40];
        snprintf(b, sizeof(b), T("%d ml im Glas"), (int)lroundf(g));
        ui_label_update(xn_sub, b);
      }
      break;
    case X_DRINK:
      ui_label_update(xn_status, "Ex!");
      show_time((now - xn_up) / 1000.0f, C_TEXT);
      if (raw >= LIFT_G) {
        xn_down = now;
        xn_state = X_SETTLE;
      }
      break;
    case X_SETTLE:
      show_time((xn_down - xn_up) / 1000.0f, C_TEXT);
      if (raw < LIFT_G) {  // doch nur kurz abgesetzt: Zeit läuft weiter
        xn_state = X_DRINK;
      } else if (st && g >= LIFT_G && now - xn_down >= SETTLE_MS) {
        xn_finish();
      } else {
        ui_label_update(xn_status, "Gestoppt – misst …");
      }
      break;
    default:
      break;
  }
}

static lv_obj_t *page_ex_turn_create() {
  lv_obj_t *s = ui_screen_create();
  char b[40];
  snprintf(b, sizeof(b), T("Ex · %d / %d"), x_player + 1, players_pick_count());
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *n = ui_label(s, xname(x_player), &font_sg_34, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -104);

  xn_status = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(xn_status, LV_ALIGN_CENTER, 0, -62);

  xn_big = ui_label(s, "0,0", &font_sg_80, C_FAINT);
  lv_obj_align(xn_big, LV_ALIGN_CENTER, 0, 4);
  lv_obj_t *u = ui_label(s, "s", &font_sg_18, C_MUTED);
  lv_obj_align(u, LV_ALIGN_CENTER, 0, 60);

  xn_sub = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(xn_sub, LV_ALIGN_CENTER, 0, 88);

  xn_btn = ui_btn(s, x_player + 1 < players_pick_count() ? "Weiter" : "Endstand", BTN_PRIMARY);
  lv_obj_align(xn_btn, LV_ALIGN_CENTER, 0, 138);
  lv_obj_add_event_cb(xn_btn, xn_next_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(xn_btn, LV_OBJ_FLAG_HIDDEN);

  xn_toast = ui_toast_create(s);
  lv_obj_align(xn_toast, LV_ALIGN_CENTER, 0, 138);

  xn_state = X_PLACE;
  ui_page_timer(s, xn_timer_cb, 50);
  xn_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Endstand: schnellste Zeit gewinnt
// ------------------------------------------------------------
static void xb_new_cb(lv_event_t *e) {
  ui_switch_page(page_ex_create());
}

static bool ex_better(int a, int b) {
  bool va = x_ml[a] >= EX_MIN_ML, vb = x_ml[b] >= EX_MIN_ML;
  if (va != vb) return va;
  return x_time[a] < x_time[b];
}

static lv_obj_t *page_ex_board_create() {
  int n = players_pick_count();
  int order[PLAYER_MAX];
  for (int i = 0; i < n; i++) order[i] = i;
  // gültige Versuche (ab EX_MIN_ML) vor ungültigen, darin die schnellste Zeit zuerst
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && ex_better(order[j], order[j - 1]); j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }

  // Spieldatei, Bestenliste, Protokoll
  char line[200], day[11], note[48], v[32];
  log_today(day);
  int h, mi;
  hal_time(&h, &mi);
  int k = snprintf(line, sizeof(line), "%s %02d:%02d;Ex", day[0] ? day : "ohne Datum", h, mi);
  for (int i = 0; i < n && k < (int)sizeof(line) - 40; i++) {
    fmt_result(v, sizeof(v), x_time[order[i]], x_ml[order[i]]);
    k += snprintf(line + k, sizeof(line) - k, ";%s:%s", xname(order[i]), v);
  }
  storage_append(DIR_GAME "/spiele.txt", line);
  for (int i = 0; i < n; i++)
    if (x_ml[i] >= EX_MIN_ML) score_add2(EX_GAME, xname(i), x_time[i], "s", x_ml[i], "ml");
  snprintf(note, sizeof(note), T("Ex: Sieger %s"), xname(order[0]));
  log_add(x_time[order[0]], note);
  sound_play(SND_DONE);

  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, n > 1 ? "Sieg!" : "Ergebnis", &font_sg_24, C_ACCENT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_t *w = ui_label(s, xname(order[0]), &font_sg_34, C_TEXT);
  lv_obj_align(w, LV_ALIGN_CENTER, 0, -104);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 280, 170);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  for (int r = 0; r < n; r++) {
    int i = order[r];
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 280, 40);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    char b[40];
    snprintf(b, sizeof(b), "%d · %.10s", r + 1, xname(i));
    lv_obj_t *l = ui_label(row, b, &font_sg_18, r == 0 ? C_ACCENT : C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    bool valid = x_ml[i] >= EX_MIN_ML;
    if (valid) fmt_result(b, sizeof(b), x_time[i], x_ml[i]);
    else snprintf(b, sizeof(b), T("%d ml · zählt nicht"), (int)lroundf(x_ml[i]));
    lv_obj_t *val = ui_label(row, b, &font_sg_14, !valid ? C_FAINT : (r == 0 ? C_ACCENT : C_MUTED));
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  lv_obj_t *h2 = ui_label(s, "Zeit · getrunken", &font_sg_14, C_FAINT);
  lv_obj_align(h2, LV_ALIGN_CENTER, 0, 104);

  lv_obj_t *b = ui_btn(s, "Neues Spiel", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 144);
  lv_obj_add_event_cb(b, xb_new_cb, LV_EVENT_CLICKED, NULL);
  return s;
}
