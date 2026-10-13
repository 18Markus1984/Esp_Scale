// ============================================================
//  Setup -> Ton -> Wörter aufnehmen
//
//  Aufnahmen für das eigene Sprachmodell: Wort wählen, antippen,
//  nach dem Piepton sprechen. Jede Aufnahme landet als WAV auf der
//  SD-Karte unter /Waage/Sprache/Aufnahmen/<wort>/a001.wav …
//  Mit dem Werkzeug im Ordner Sprachmodell (kws_train.py
//  --aufnahmen) fließen sie ins Training ein. 10 bis 20 Aufnahmen
//  je Wort, verschieden laut und aus verschiedenen Abständen,
//  helfen am meisten.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "voice.h"
#include "sound.h"
#include "storage.h"
#include <stdio.h>

static int r_word = 0;
static lv_obj_t *r_ring, *r_count, *r_word_lbl, *r_state, *r_btn, *r_ic, *r_level, *r_stop, *r_series;
static int r_prev_state = -9;

static lv_obj_t *page_voice_words_create();

// "1 Aufnahme", "12 Aufnahmen"
static void count_text(char *b, int len, int n) {
  if (n == 1) snprintf(b, len, "%s", T("1 Aufnahme"));
  else snprintf(b, len, T("%d Aufnahmen"), n);
}

static void r_show_count() {
  char b[40];
  count_text(b, sizeof(b), voice_word_count(r_word));
  ui_label_update(r_count, b);
}

static void r_timer_cb(lv_timer_t *t) {
  int st = voice_rec_state();
  // Pegel: dünner Balken, damit man sieht, dass das Mikrofon hört
  int lv = (int)(voice_level() * 4.0f * 200.0f);  // leise Mikrofone: etwas verstärkt anzeigen
  if (lv > 200) lv = 200;
  if (lv < 4) lv = 4;
  lv_obj_set_width(r_level, lv);
  int v = (int)(voice_rec_progress() * 1000.0f);
  if (lv_arc_get_value(r_ring) != v) lv_arc_set_value(r_ring, v);
  if (st == r_prev_state) return;
  r_prev_state = st;
  bool bg = voice_rec_ms(r_word) > 3000 + 500;
  bool series = voice_rec_series() && st >= 1 && st <= 3;
  const char *txt = bg ? "Tippen, dann 10 s nichts sagen" : "Tippen, nach dem Ton sprechen";
  lv_color_t c = C_MUTED;
  if (st == 1) txt = "gleich …";
  else if (st == 2) {
    txt = bg ? "Aufnahme läuft …" : (series ? "Wort mehrmals sagen, mit Pausen" : "Jetzt sprechen");
    c = C_ACCENT;
  } else if (st == 3) txt = "speichert …";
  else if (st == 4) {
    txt = "Gespeichert";
    c = C_ACCENT;
    r_show_count();
  } else if (st < 0) {
    txt = "Nicht gespeichert (SD-Karte?)";
    c = C_DANGER;
  }
  ui_label_update(r_state, txt);
  lv_obj_set_style_text_color(r_state, c, 0);
  bool busy = st >= 1 && st <= 3;
  bool can_stop = series && st == 2;  // Serie: Knopf beendet sie vorzeitig
  if (busy && !can_stop) lv_obj_add_state(r_btn, LV_STATE_DISABLED);
  else lv_obj_clear_state(r_btn, LV_STATE_DISABLED);
  // während der Aufnahme: Knopf und Mikrofon grau (Ring und "Jetzt sprechen" zeigen die Aufnahme)
  lv_obj_set_style_text_color(r_ic, busy ? C_MUTED : C_ON_PRIMARY, 0);
  if (can_stop) {
    lv_obj_add_flag(r_ic, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(r_stop, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(r_ic, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(r_stop, LV_OBJ_FLAG_HIDDEN);
  }
  if (busy || bg) lv_obj_add_flag(r_series, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(r_series, LV_OBJ_FLAG_HIDDEN);
  if (st == 0) lv_arc_set_value(r_ring, 0);
}

static void r_start(bool series);

static void r_rec_cb(lv_event_t *e) {
  if (voice_rec_series() && voice_rec_state() == 2) {  // Serie vorzeitig beenden
    sound_play(SND_CLICK);
    voice_rec_stop();
    return;
  }
  r_start(false);
}

static void r_series_cb(lv_event_t *e) {
  r_start(true);
}

static void r_start(bool series) {
  if (voice_rec_state() >= 1 && voice_rec_state() <= 3) return;
  if (!storage_ok()) {
    sound_play(SND_WARN);
    ui_label_update(r_state, "Keine SD-Karte");
    lv_obj_set_style_text_color(r_state, C_DANGER, 0);
    return;
  }
  sound_play_tone(SND_PARK);  // Piepton, die Aufnahme beginnt kurz danach
  if (!voice_rec_start(r_word, series)) sound_play(SND_WARN);
}

static void session_del_cb(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) voice_rec_session(false);
}

static lv_obj_t *page_voice_rec_word() {
  lv_obj_t *s = ui_screen_create();
  voice_rec_session(true);
  lv_obj_add_event_cb(s, session_del_cb, LV_EVENT_DELETE, NULL);
  r_ring = ui_ring(s, 396);
  lv_obj_set_style_arc_width(r_ring, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(r_ring, 8, LV_PART_INDICATOR);

  r_count = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(r_count, LV_ALIGN_TOP_MID, 0, 62);

  // das Wort groß, in Anführungszeichen
  char b[48];
  snprintf(b, sizeof(b), "„%s“", T(voice_word_text(r_word)));
  r_word_lbl = ui_label(s, b, &font_sg_34, C_TEXT);
  lv_obj_set_width(r_word_lbl, 300);
  lv_obj_set_style_text_align(r_word_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(r_word_lbl, LV_LABEL_LONG_WRAP);
  lv_obj_align(r_word_lbl, LV_ALIGN_TOP_MID, 0, 100);

  // großer runder Aufnahme-Knopf mit Mikrofon
  r_btn = lv_btn_create(s);
  lv_obj_remove_style_all(r_btn);
  lv_obj_set_size(r_btn, 104, 104);
  lv_obj_set_style_radius(r_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(r_btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(r_btn, C_ACCENT, 0);
  lv_obj_set_style_bg_color(r_btn, lv_color_mix(C_ACCENT, C_BG, 160), LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(r_btn, C_SURFACE, LV_STATE_DISABLED);
  lv_obj_align(r_btn, LV_ALIGN_CENTER, 0, 30);
  r_ic = ui_label(r_btn, ICON_MIKRO, &font_icons_36, C_ON_PRIMARY);
  lv_obj_center(r_ic);
  // Stopp-Zeichen (Quadrat) während einer Serie
  r_stop = ui_box(r_btn);
  lv_obj_set_size(r_stop, 30, 30);
  lv_obj_set_style_radius(r_stop, 5, 0);
  lv_obj_set_style_bg_opa(r_stop, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(r_stop, C_ON_PRIMARY, 0);
  lv_obj_center(r_stop);
  lv_obj_add_flag(r_stop, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(r_btn, r_rec_cb, LV_EVENT_CLICKED, NULL);

  r_state = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(r_state, LV_ALIGN_CENTER, 0, 96);

  // Pegelbalken
  lv_obj_t *track = ui_box(s);
  lv_obj_set_size(track, 200, 6);
  lv_obj_set_style_radius(track, 3, 0);
  lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(track, C_TRACK, 0);
  lv_obj_align(track, LV_ALIGN_CENTER, 0, 126);
  r_level = ui_box(track);
  lv_obj_set_size(r_level, 4, 6);
  lv_obj_set_style_radius(r_level, 3, 0);
  lv_obj_set_style_bg_opa(r_level, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(r_level, C_TEXT2, 0);
  lv_obj_align(r_level, LV_ALIGN_LEFT_MID, 0, 0);


  // Serienaufnahme: 30 s am Stück, das Wort mehrmals sagen (spart viel Tippen)
  r_series = ui_btn(s, "Serie 30 s", BTN_NORMAL);
  lv_obj_set_size(r_series, 132, 40);
  lv_obj_set_ext_click_area(r_series, 4);
  lv_obj_set_style_text_font(lv_obj_get_child(r_series, 0), &font_sg_18, 0);
  lv_obj_align(r_series, LV_ALIGN_CENTER, 0, 155);
  lv_obj_add_event_cb(r_series, r_series_cb, LV_EVENT_CLICKED, NULL);

  r_prev_state = -9;
  r_show_count();
  ui_page_timer(s, r_timer_cb, 50);
  r_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Liste der Wörter mit Anzahl der Aufnahmen
// ------------------------------------------------------------
#define WMAX 48
static ui_list_item_t w_items[WMAX];
static char w_sub[WMAX][32];

static void w_pick_cb(int i) {
  r_word = i;
  ui_switch_page(page_voice_rec_word());
  ui_set_back(page_voice_words_create);  // Wischen nach rechts: zurück zur Liste
}

static lv_obj_t *page_voice_words_create() {
  lv_obj_t *s = ui_screen_create();
  voice_rec_session(true);  // Mikrofon bereithalten (Pegel, schneller Start)
  lv_obj_add_event_cb(s, session_del_cb, LV_EVENT_DELETE, NULL);
  int n = voice_words();
  if (n > WMAX) n = WMAX;
  for (int i = 0; i < n; i++) {
    w_items[i].title = voice_word_text(i);
    count_text(w_sub[i], sizeof(w_sub[i]), voice_word_count(i));
    w_items[i].sub = w_sub[i];
    w_items[i].icon = NULL;
  }
  ui_curved_list(s, w_items, n, w_pick_cb);
  lv_obj_t *t = ui_label(s, "Wörter aufnehmen", &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 28);
  ui_set_back(page_setup_back);  // Wischen nach rechts: zurück in Setup -> Ton (auch von der Wortseite aus)
  return s;
}

lv_obj_t *page_voice_rec_create() {
  return page_voice_words_create();
}
