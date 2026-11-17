// ============================================================
//  Modi -> Küche -> Kaffee
//
//  Filter (Handfilter/Pour-Over):
//    Verhältnis wählen (z. B. 1:16) -> Gefäß mit Filter auflegen, Tara ->
//    Kaffeepulver einfüllen, übernehmen -> Wasser aufgießen:
//    Bloom (doppelte Kaffeemenge, bis BLOOM_S nach dem ersten Wasser),
//    dann bis zur Zielmenge. Zeit startet mit dem ersten Wasser.
//  Espresso:
//    Verhältnis (z. B. 1:2) -> Siebträger auflegen, Tara -> Pulver einfüllen,
//    übernehmen -> Tasse auflegen, Tara -> Start oder erster Tropfen startet
//    die Zeit -> bei der Zielausbeute ein Ton.
//  Durchfluss g/s über die letzte Sekunde. Ergebnis landet im Protokoll.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "sound.h"
#include "data.h"
#include "hal.h"
#include <stdio.h>
#include <math.h>

#define BLOOM_S 35          // Bloom-Phase in s ab dem ersten Wasser
#define START_G 2.0f        // Filter: ab so viel Wasser läuft die Zeit
#define DRIP_G 0.4f         // Espresso: erster Tropfen
#define MIN_DOSE_G 3.0f

static int k_mode = 0;                 // 0 Filter, 1 Espresso
static float k_ratio[2] = { 16.0f, 2.0f };
static float k_dose = 0;               // g Kaffee
static float k_target = 0;             // g Wasser bzw. Ausbeute

static lv_obj_t *page_dose_create();
static lv_obj_t *page_brew_create();

static void fmt_ratio(char *b, int len, float r) {
  char t[12];
  if (fabsf(r - roundf(r)) < 0.01f) snprintf(t, sizeof(t), "%d", (int)lroundf(r));
  else ui_fmt_1dec(t, sizeof(t), r);
  snprintf(b, len, "1:%s", t);
}

static void fmt_time(char *b, int len, uint32_t ms) {
  uint32_t s = ms / 1000;
  snprintf(b, len, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

// ------------------------------------------------------------
//  Einrichten: Filter oder Espresso, Verhältnis
// ------------------------------------------------------------
static lv_obj_t *ks_mode[2], *ks_ratio, *ks_info;

static void ks_show() {
  for (int i = 0; i < 2; i++) {
    lv_obj_set_style_bg_color(ks_mode[i], i == k_mode ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(ks_mode[i], 0), i == k_mode ? C_BG : C_TEXT2, 0);
  }
  char b[16];
  fmt_ratio(b, sizeof(b), k_ratio[k_mode]);
  lv_label_set_text(ks_ratio, b);
  lv_label_set_text(ks_info, k_mode == 0 ? T("Wasser = Kaffee × Verhältnis, mit Bloom")
                                          : T("Ausbeute in der Tasse = Kaffee × Verhältnis"));
}

static void ks_mode_cb(lv_event_t *e) {
  k_mode = (int)(intptr_t)lv_event_get_user_data(e);
  ks_show();
}

static void ks_adj_cb(lv_event_t *e) {
  int up = (int)(intptr_t)lv_event_get_user_data(e);
  float &r = k_ratio[k_mode];
  float lo = k_mode == 0 ? 10.0f : 1.0f, hi = k_mode == 0 ? 20.0f : 4.0f;
  r += up ? 0.5f : -0.5f;
  if (r < lo) r = lo;
  if (r > hi) r = hi;
  sound_play(SND_CLICK);
  ks_show();
}

static void ks_go_cb(lv_event_t *e) {
  k_dose = 0;
  scale_tare();
  ui_switch_page(page_dose_create());
}

lv_obj_t *page_kaffee_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Kaffee", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, -96);
  const char *names[2] = { "Filter", "Espresso" };
  for (int i = 0; i < 2; i++) {
    ks_mode[i] = ui_btn(row, names[i], BTN_NORMAL);
    lv_obj_set_height(ks_mode[i], 42);
    lv_obj_set_style_pad_hor(ks_mode[i], 16, 0);
    lv_obj_add_event_cb(ks_mode[i], ks_mode_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  lv_obj_t *rl = ui_label(s, "Verhältnis", &font_sg_14, C_MUTED);
  lv_obj_align(rl, LV_ALIGN_CENTER, 0, -50);
  ks_ratio = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(ks_ratio, LV_ALIGN_CENTER, 0, -14);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_align(m, LV_ALIGN_CENTER, -112, -14);
  lv_obj_add_event_cb(m, ks_adj_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_align(p, LV_ALIGN_CENTER, 112, -14);
  lv_obj_add_event_cb(p, ks_adj_cb, LV_EVENT_CLICKED, (void *)1);

  ks_info = ui_label(s, "", &font_sg_14, C_FAINT);
  lv_obj_set_width(ks_info, 270);
  lv_label_set_long_mode(ks_info, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(ks_info, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ks_info, LV_ALIGN_CENTER, 0, 30);

  lv_obj_t *go = ui_btn(s, "Los", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 40, 0);
  lv_obj_align(go, LV_ALIGN_CENTER, 0, 100);
  lv_obj_add_event_cb(go, ks_go_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Waage vorher leer lassen", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 148);
  ks_show();
  return s;
}

// ------------------------------------------------------------
//  Kaffee abwiegen
// ------------------------------------------------------------
static lv_obj_t *kd_weight, *kd_info, *kd_ok;

static void kd_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  char b[48], w[16];
  ui_fmt_1dec(w, sizeof(w), g < 0 ? 0 : g);
  ui_label_update(kd_weight, w);
  if (g >= MIN_DOSE_G) {
    snprintf(b, sizeof(b), k_mode == 0 ? T("ergibt %d g Wasser") : T("ergibt %d g in der Tasse"),
             (int)lroundf(g * k_ratio[k_mode]));
    lv_obj_clear_state(kd_ok, LV_STATE_DISABLED);
  } else {
    snprintf(b, sizeof(b), "%s", k_mode == 0 ? T("Gefäß mit Filter auflegen, Tara") : T("Siebträger auflegen, Tara"));
    lv_obj_add_state(kd_ok, LV_STATE_DISABLED);
  }
  ui_label_update(kd_info, b);
}

static void kd_tara_cb(lv_event_t *e) {
  scale_tare();
  sound_play(SND_CLICK);
}

static void kd_ok_cb(lv_event_t *e) {
  float g = scale_net();
  if (g < MIN_DOSE_G) return;
  k_dose = g;
  k_target = g * k_ratio[k_mode];
  if (k_mode == 0) scale_tare();  // Filter: das Pulver bleibt liegen, ab jetzt zählt nur das Wasser
  sound_play(SND_TICK);
  ui_switch_page(page_brew_create());
}

static lv_obj_t *page_dose_create() {
  lv_obj_t *s = ui_screen_create();
  char b[40], r[12];
  fmt_ratio(r, sizeof(r), k_ratio[k_mode]);
  snprintf(b, sizeof(b), "%s · %s", k_mode == 0 ? T("Filter") : T("Espresso"), r);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_t *h = ui_label(s, "Kaffeepulver einfüllen", &font_sg_24, C_TEXT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -104);

  kd_weight = ui_label(s, "0,0", &font_sg_80, C_TEXT);
  lv_obj_align(kd_weight, LV_ALIGN_CENTER, 0, -30);
  kd_info = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_align(kd_info, LV_ALIGN_CENTER, 0, 30);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 96);
  lv_obj_t *ta = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_add_event_cb(ta, kd_tara_cb, LV_EVENT_CLICKED, NULL);
  kd_ok = ui_btn(row, "Übernehmen", BTN_PRIMARY);
  lv_obj_add_event_cb(kd_ok, kd_ok_cb, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, kd_timer_cb, 150);
  kd_timer_cb(NULL);
  ui_keep_open(s);
  return s;
}

// ------------------------------------------------------------
//  Aufgießen (Filter) bzw. Shot (Espresso)
// ------------------------------------------------------------
typedef enum { K_CUP, K_READY, K_RUN, K_REACHED, K_DONE } kstate_t;
static kstate_t kb_state;
static uint32_t kb_start, kb_end;
static bool kb_bloom_done;
static float kb_hist_g[5];
static uint32_t kb_hist_t[5];
static int kb_hist_i;
static uint32_t kb_hist_last;
static lv_obj_t *kb_ring, *kb_time, *kb_big, *kb_goal, *kb_phase, *kb_flow, *kb_btn, *kb_btn2;

static float kb_flow_gs(float g, uint32_t now) {
  if (now - kb_hist_last >= 250) {  // alle 250 ms merken, Fenster 1 s
    kb_hist_last = now;
    kb_hist_g[kb_hist_i] = g;
    kb_hist_t[kb_hist_i] = now;
    kb_hist_i = (kb_hist_i + 1) % 5;
  }
  int old = kb_hist_i;  // ältester Eintrag
  if (!kb_hist_t[old]) return 0;
  float dt = (now - kb_hist_t[old]) / 1000.0f;
  if (dt < 0.5f) return 0;
  float f = (g - kb_hist_g[old]) / dt;
  return f < 0 ? 0 : f;
}

static void kb_finish() {
  if (kb_state == K_DONE) return;
  if (kb_state == K_RUN || kb_state == K_REACHED) kb_end = hal_millis();
  kb_state = K_DONE;
  float g = scale_net();
  char t[12], r[16], note[64], b[64];
  fmt_time(t, sizeof(t), kb_end - kb_start);
  float real = k_dose > 0 ? g / k_dose : 0;
  fmt_ratio(r, sizeof(r), real);
  if (k_mode == 0) snprintf(note, sizeof(note), T("Filterkaffee %d g · %d g Wasser · %s"), (int)lroundf(k_dose),
                            (int)lroundf(g), t);
  else snprintf(note, sizeof(note), T("Espresso %d g · %d g · %s"), (int)lroundf(k_dose), (int)lroundf(g), t);
  log_add(g, note);
  snprintf(b, sizeof(b), T("Fertig · %s · Protokoll"), r);
  ui_label_update(kb_phase, b);
  lv_obj_set_style_text_color(kb_phase, C_ACCENT, 0);
  lv_label_set_text(lv_obj_get_child(kb_btn, 0), T("Nochmal"));
  lv_obj_add_flag(kb_btn2, LV_OBJ_FLAG_HIDDEN);
  sound_play(SND_DONE);
}

static void kb_btn_cb(lv_event_t *e) {
  if (kb_state == K_DONE) {
    ui_switch_page(page_kaffee_create());
    return;
  }
  if (kb_state == K_CUP) {  // Espresso: Tasse steht, Tara
    scale_tare();
    kb_state = K_READY;
    sound_play(SND_CLICK);
    lv_label_set_text(lv_obj_get_child(kb_btn, 0), T("Start"));
    return;
  }
  if (kb_state == K_READY) {  // Pumpe läuft: Zeit startet jetzt
    kb_start = hal_millis();
    kb_state = K_RUN;
    sound_play(SND_TICK);
    lv_label_set_text(lv_obj_get_child(kb_btn, 0), T("Fertig"));
    return;
  }
  kb_finish();
}

static void kb_tara_cb(lv_event_t *e) {
  scale_tare();
  sound_play(SND_CLICK);
}

static void kb_timer_cb(lv_timer_t *tm) {
  uint32_t now = hal_millis();
  float g = scale_net();
  float shown = g < 0 ? 0 : g;
  char b[48], w[16];
  ui_fmt_1dec(w, sizeof(w), shown);
  ui_label_update(kb_big, w);

  // Zeit läuft los: Filter beim ersten Wasser, Espresso beim ersten Tropfen (oder "Start")
  if (kb_state == K_READY && g >= (k_mode == 0 ? START_G : DRIP_G)) {
    kb_start = now;
    kb_state = K_RUN;
    sound_play(SND_TICK);
    lv_label_set_text(lv_obj_get_child(kb_btn, 0), T("Fertig"));
  }
  if (kb_state == K_RUN && g >= k_target - 0.3f) {
    kb_state = K_REACHED;
    sound_play(SND_REACHED);
  }

  uint32_t el = (kb_state == K_RUN || kb_state == K_REACHED) ? now - kb_start : (kb_state == K_DONE ? kb_end - kb_start : 0);
  fmt_time(b, sizeof(b), el);
  ui_label_update(kb_time, b);

  int v = (k_target > 0 && kb_state != K_CUP) ? (int)(shown * 1000 / k_target) : 0;  // vor Tara der Tasse: leer
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(kb_ring) != v) lv_arc_set_value(kb_ring, v);
  lv_obj_set_style_text_color(kb_big, kb_state == K_REACHED || (kb_state == K_DONE && g >= k_target - 0.3f) ? C_ACCENT : C_TEXT, 0);

  if (kb_state == K_RUN || kb_state == K_REACHED) {
    char f[12];
    ui_fmt_1dec(f, sizeof(f), kb_flow_gs(g, now));
    snprintf(b, sizeof(b), "%s g/s", f);
    ui_label_update(kb_flow, b);
  }
  if (kb_state == K_DONE) return;

  // Hinweis, was gerade dran ist
  if (k_mode == 0) {
    float bloom = k_dose * 2;
    if (kb_state == K_READY) snprintf(b, sizeof(b), T("Bloom: bis %d g aufgießen"), (int)lroundf(bloom));
    else if (kb_state == K_REACHED) snprintf(b, sizeof(b), "%s", T("Ziel erreicht – durchlaufen lassen"));
    else if (el < BLOOM_S * 1000UL && g < bloom + 15) {
      if (g < bloom - 1) snprintf(b, sizeof(b), T("Bloom: bis %d g aufgießen"), (int)lroundf(bloom));
      else snprintf(b, sizeof(b), T("Bloom – warten %u s"), (unsigned)(BLOOM_S - el / 1000));
    } else {
      if (!kb_bloom_done) {
        kb_bloom_done = true;
        sound_play(SND_TICK);
      }
      snprintf(b, sizeof(b), T("Aufgießen bis %d g"), (int)lroundf(k_target));
    }
  } else {
    if (kb_state == K_CUP) snprintf(b, sizeof(b), "%s", T("Tasse auflegen, dann Tara"));
    else if (kb_state == K_READY) snprintf(b, sizeof(b), "%s", T("Start oder erster Tropfen"));
    else if (kb_state == K_REACHED) snprintf(b, sizeof(b), "%s", T("Ziel erreicht – stoppen!"));
    else snprintf(b, sizeof(b), "%s", T("Läuft"));
  }
  ui_label_update(kb_phase, b);
}

static lv_obj_t *page_brew_create() {
  lv_obj_t *s = ui_screen_create();
  kb_ring = ui_ring(s, 396);
  char b[48], r[12];
  fmt_ratio(r, sizeof(r), k_ratio[k_mode]);
  snprintf(b, sizeof(b), "%s · %d g · %s", k_mode == 0 ? T("Filter") : T("Espresso"), (int)lroundf(k_dose), r);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  kb_time = ui_label(s, "0:00", &font_sg_24, C_TEXT);
  lv_obj_align(kb_time, LV_ALIGN_CENTER, 0, -108);
  kb_big = ui_label(s, "0,0", &font_sg_80, C_TEXT);
  lv_obj_align(kb_big, LV_ALIGN_CENTER, 0, -40);
  snprintf(b, sizeof(b), T("von %d g"), (int)lroundf(k_target));
  kb_goal = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(kb_goal, LV_ALIGN_CENTER, 0, 14);
  kb_phase = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_align(kb_phase, LV_ALIGN_CENTER, 0, 44);
  kb_flow = ui_label(s, "", &font_sg_14, C_FAINT);
  lv_obj_align(kb_flow, LV_ALIGN_CENTER, 0, 70);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 118);
  kb_btn2 = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_add_event_cb(kb_btn2, kb_tara_cb, LV_EVENT_CLICKED, NULL);
  kb_state = k_mode == 0 ? K_READY : K_CUP;
  kb_btn = ui_btn(row, k_mode == 0 ? "Fertig" : "Tara + bereit", BTN_PRIMARY);
  lv_obj_add_event_cb(kb_btn, kb_btn_cb, LV_EVENT_CLICKED, NULL);
  if (k_mode == 1) lv_obj_add_flag(kb_btn2, LV_OBJ_FLAG_HIDDEN);  // Espresso: Tara steckt im Hauptknopf

  kb_start = kb_end = 0;
  kb_bloom_done = false;
  kb_hist_i = 0;
  kb_hist_last = 0;
  for (int i = 0; i < 5; i++) kb_hist_t[i] = 0;
  ui_page_timer(s, kb_timer_cb, 100);
  kb_timer_cb(NULL);
  ui_keep_open(s);
  return s;
}
