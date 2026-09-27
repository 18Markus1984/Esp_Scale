// ============================================================
//  Modus "Zählen": Stückgewicht lernen, Teile zählen,
//  Stückzahl per Bluetooth-Tastatur an den PC senden
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "ble_kbd.h"
#include "sound.h"
#include "data.h"
#include <stdio.h>
#include <math.h>

#define UNSICHER_ANTEIL 0.3f  // Abweichung vom ganzen Stück, ab der gewarnt wird

// Bleibt erhalten, solange die Waage läuft
static float piece_g = 0.0f;  // Stückgewicht, 0 = noch keine Referenz
static int ref_count = 10;    // Anzahl Teile für die Referenz
static int sent_lines = 0;    // gesendete Werte in dieser Sitzung

static lv_obj_t *page_ref_create();
static lv_obj_t *page_count_create();
static bool bt_from_count = false;  // Hilfe aus dem Zählmodus geöffnet?

// ------------------------------------------------------------
//  Schritt 1: Referenz lernen
// ------------------------------------------------------------
static lv_obj_t *zr_count, *zr_weight, *zr_ok;
static lv_timer_t *zr_timer;

static void zr_show_count() {
  char b[16];
  snprintf(b, sizeof(b), T("%d Stk"), ref_count);
  lv_label_set_text(zr_count, b);
}

static void zr_minus_cb(lv_event_t *e) {
  if (ref_count > 1) ref_count -= (ref_count > 10 ? 5 : 1);
  zr_show_count();
}

static void zr_plus_cb(lv_event_t *e) {
  if (ref_count < 100) ref_count += (ref_count >= 10 ? 5 : 1);
  zr_show_count();
}

static void zr_tara_cb(lv_event_t *e) {
  scale_tare();
}

static void zr_ok_cb(lv_event_t *e) {
  float g = scale_net();
  if (!scale_stable() || g < 0.5f) return;
  piece_g = g / ref_count;
  ui_switch_page(page_count_create());
}

static void zr_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  char w[16], b[40];
  ui_fmt_weight(w, sizeof(w), g);
  snprintf(b, sizeof(b), T("%s g gemessen"), w);
  ui_label_update(zr_weight, b);

  bool ready = scale_stable() && g >= 0.5f;
  if (ready) lv_obj_clear_state(zr_ok, LV_STATE_DISABLED);
  else lv_obj_add_state(zr_ok, LV_STATE_DISABLED);
}

static lv_obj_t *page_ref_create() {
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *t = ui_label(s, "Referenz lernen", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *hint = ui_label(s, "Teile auflegen", &font_sg_24, C_TEXT);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, -95);

  zr_count = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(zr_count, LV_ALIGN_CENTER, 0, -30);
  lv_obj_t *minus = ui_round_btn(s, "−", zr_minus_cb);
  lv_obj_align(minus, LV_ALIGN_CENTER, -120, -30);
  lv_obj_t *plus = ui_round_btn(s, "+", zr_plus_cb);
  lv_obj_align(plus, LV_ALIGN_CENTER, 120, -30);

  zr_weight = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(zr_weight, LV_ALIGN_CENTER, 0, 30);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 100);
  lv_obj_t *tara = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_add_event_cb(tara, zr_tara_cb, LV_EVENT_CLICKED, NULL);
  zr_ok = ui_btn(row, "Übernehmen", BTN_PRIMARY);
  lv_obj_add_event_cb(zr_ok, zr_ok_cb, LV_EVENT_CLICKED, NULL);

  zr_show_count();
  zr_timer = ui_page_timer(s, zr_timer_cb, 100);
  zr_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Schritt 2: Zählen und senden
// ------------------------------------------------------------
static lv_obj_t *zc_bt, *zc_count, *zc_info, *zc_toast;
static lv_timer_t *zc_timer;
static int zc_prev_bt, zc_prev_unsure;
static int zc_last_count;

static void zc_ref_cb(lv_event_t *e) {
  ui_switch_page(page_ref_create());
}

static void zc_help_cb(lv_event_t *e) {
  bt_from_count = true;
  ui_switch_page(page_bluetooth_create());
}

static void zc_send_cb(lv_event_t *e) {
  if (!scale_stable()) {
    sound_play(SND_WARN);
    ui_toast_show(zc_toast, "Noch nicht stabil", C_WARN);
    return;
  }
  if (!ble_kbd_connected()) {  // nicht verbunden -> Anleitung zeigen
    zc_help_cb(e);
    return;
  }
  char b[40];
  snprintf(b, sizeof(b), "%d", zc_last_count);
  if (ble_kbd_send_line(b)) {
    sent_lines++;
    sound_play(SND_SAVE);
    sound_speak_count(zc_last_count);
    snprintf(b, sizeof(b), T("Gesendet · Nr. %d"), sent_lines);
    ui_toast_show(zc_toast, b, C_ACCENT);
  }
}

static void zc_timer_cb(lv_timer_t *t) {
  float pieces = scale_net() / piece_g;
  int count = (int)lroundf(pieces);
  if (count < 0) count = 0;
  zc_last_count = count;

  char b[48];
  snprintf(b, sizeof(b), "%d", count);
  ui_label_update(zc_count, b);

  snprintf(b, sizeof(b), T("Zählen: %d Stk"), count);  // automatisch mitschreiben
  if (track_update(scale_net(), scale_stable(), b)) ui_toast_show(zc_toast, "Im Protokoll gespeichert", C_ACCENT);

  // Liegt das Gewicht deutlich zwischen zwei Stückzahlen?
  int unsure = (count > 0 && fabsf(pieces - count) > UNSICHER_ANTEIL) ? 1 : 0;
  if (unsure != zc_prev_unsure) {
    zc_prev_unsure = unsure;
    if (unsure) {
      ui_label_update(zc_info, "unsicher · Referenz prüfen");
      lv_obj_set_style_text_color(zc_info, C_WARN, 0);
    } else {
      char w[16];
      snprintf(w, sizeof(w), "%.2f", piece_g);  // 2 Nachkommastellen für kleine Teile
      for (char *p = w; *p; p++) if (*p == '.') *p = ',';
      snprintf(b, sizeof(b), T("Stück · %s g/Stk"), w);
      ui_label_update(zc_info, b);
      lv_obj_set_style_text_color(zc_info, C_MUTED, 0);
    }
  }

  // Bluetooth-Status: 0 = aus, 1 = wartet, 2 = verbunden
  int bt = !ble_kbd_enabled() ? 0 : (ble_kbd_connected() ? 2 : 1);
  if (bt != zc_prev_bt) {
    zc_prev_bt = bt;
    if (bt == 2) ui_chip_set(zc_bt, "Bluetooth verbunden", C_ACCENT, false);
    else if (bt == 1) ui_chip_set(zc_bt, "Nicht gekoppelt · Hilfe ›", C_WARN, false);
    else ui_chip_set(zc_bt, "Bluetooth aus", C_FAINT, false);
  }
}

static lv_obj_t *page_count_create() {
  lv_obj_t *s = ui_screen_create();

  zc_bt = ui_chip(s, "", C_FAINT);
  lv_obj_align(zc_bt, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_add_flag(zc_bt, LV_OBJ_FLAG_CLICKABLE);  // Antippen = Anleitung
  lv_obj_set_ext_click_area(zc_bt, 10);
  lv_obj_add_event_cb(zc_bt, zc_help_cb, LV_EVENT_CLICKED, NULL);

  zc_count = ui_label(s, "0", &font_sg_80, C_TEXT);
  lv_obj_align(zc_count, LV_ALIGN_CENTER, 0, -30);

  zc_info = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(zc_info, LV_ALIGN_CENTER, 0, 36);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 100);
  lv_obj_t *ref = ui_btn(row, "Referenz", BTN_NORMAL);
  lv_obj_add_event_cb(ref, zc_ref_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *send = ui_btn(row, "Senden", BTN_PRIMARY);
  lv_obj_add_event_cb(send, zc_send_cb, LV_EVENT_CLICKED, NULL);

  zc_toast = ui_toast_create(s);
  lv_obj_align(zc_toast, LV_ALIGN_CENTER, 0, 152);

  zc_prev_bt = -1;
  track_reset();
  zc_prev_unsure = -1;
  zc_timer = ui_page_timer(s, zc_timer_cb, 100);
  zc_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Bluetooth-Anleitung (auch aus Setup -> Bluetooth erreichbar)
// ------------------------------------------------------------
static lv_obj_t *bt_chip;

static void bt_timer_cb(lv_timer_t *t) {
  if (!ble_kbd_enabled()) ui_chip_set(bt_chip, "Bluetooth ausgeschaltet", C_FAINT, false);
  else if (ble_kbd_connected()) ui_chip_set(bt_chip, "Verbunden", C_ACCENT, false);
  else ui_chip_set(bt_chip, "Sichtbar als „Waage“", C_WARN, false);
}

static void bt_done_cb(lv_event_t *e) {
  if (bt_from_count) ui_switch_page(piece_g > 0.0f ? page_count_create() : page_ref_create());
  else ui_switch_page(page_setup_back());
}

lv_obj_t *page_bluetooth_create() {
  ble_kbd_begin();
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *t = ui_label(s, "Bluetooth", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 44);

  bt_chip = ui_chip(s, "", C_WARN);
  lv_obj_align(bt_chip, LV_ALIGN_CENTER, 0, -104);

  lv_obj_t *steps = ui_label(s,
                             "1. Am PC: Einstellungen › Bluetooth\n"
                             "2. Gerät hinzufügen › Bluetooth\n"
                             "3. „Waage“ auswählen und koppeln\n"
                             "4. In Excel die Zielzelle anklicken\n"
                             "5. „Senden“ tippt Zahl + Enter",
                             &font_sg_14, C_TEXT2);
  lv_obj_set_style_text_align(steps, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_text_line_space(steps, 4, 0);
  lv_obj_align(steps, LV_ALIGN_CENTER, 0, -22);

  lv_obj_t *note = ui_label(s, "Klappt es nicht: „Waage“ am PC\nentfernen und neu koppeln",
                            &font_sg_14, C_FAINT);
  lv_obj_align(note, LV_ALIGN_CENTER, 0, 64);

  lv_obj_t *b = ui_btn(s, "Fertig", BTN_NORMAL);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 128);
  lv_obj_add_event_cb(b, bt_done_cb, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, bt_timer_cb, 500);
  bt_timer_cb(NULL);
  return s;
}

// Aus dem Setup geöffnet: "Fertig" führt zurück ins Setup
lv_obj_t *page_bluetooth_setup_create() {
  bt_from_count = false;
  return page_bluetooth_create();
}

// ------------------------------------------------------------
//  Einstieg aus dem Launcher
// ------------------------------------------------------------
lv_obj_t *page_zaehlen_create() {
  ble_kbd_begin();  // Bluetooth erst starten, wenn der Modus gebraucht wird
  return piece_g > 0.0f ? page_count_create() : page_ref_create();
}
