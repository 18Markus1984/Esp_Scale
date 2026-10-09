// ============================================================
//  Modus "Ziel" (Parkpiepser) – zugleich Vorlage für neue Seiten
//
//  Aufbau einer Seite:
//   1. Alle Zustände als static-Variablen in dieser Datei
//   2. page_xxx_create() baut den Screen und gibt ihn zurück
//   3. Timer immer im LV_EVENT_DELETE des Screens löschen
//   4. Nächster Schritt im selben Modus: ui_switch_page(...)
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "config.h"
#include "sound.h"
#include "data.h"
#include <stdio.h>

#define ZIEL_TOLERANZ_G 2.0f

// Bleibt erhalten, auch wenn die Seite geschlossen wird
static int ziel_g = 250;
static int ziel_step = 10;

// ------------------------------------------------------------
//  Schritt 1: Zielgewicht einstellen
// ------------------------------------------------------------
static lv_obj_t *z_value;
static lv_obj_t *z_step_btns[3];
static const int STEPS[3] = { 1, 10, 100 };

static lv_obj_t *page_ziel_active_create();

static void z_show() {
  char b[16];
  snprintf(b, sizeof(b), "%d g", ziel_g);
  lv_label_set_text(z_value, b);
  for (int i = 0; i < 3; i++) {
    bool on = STEPS[i] == ziel_step;
    lv_obj_set_style_bg_color(z_step_btns[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(z_step_btns[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

static void z_minus_cb(lv_event_t *e) {
  ziel_g -= ziel_step;
  if (ziel_g < 1) ziel_g = 1;
  z_show();
}

static void z_plus_cb(lv_event_t *e) {
  ziel_g += ziel_step;
  if (ziel_g > (int)WAAGE_MAX_G) ziel_g = (int)WAAGE_MAX_G;
  z_show();
}

static void z_step_cb(lv_event_t *e) {
  ziel_step = (int)(intptr_t)lv_event_get_user_data(e);
  z_show();
}

static void z_start_cb(lv_event_t *e) {
  scale_tare();
  sound_parking_reset();
  ui_switch_page(page_ziel_active_create());
}

lv_obj_t *page_ziel_create() {
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *t = ui_label(s, "Zielgewicht", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);

  z_value = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(z_value, LV_ALIGN_CENTER, 0, -40);

  lv_obj_t *minus = ui_round_btn(s, "−", z_minus_cb);
  lv_obj_align(minus, LV_ALIGN_CENTER, -125, -40);

  lv_obj_t *plus = ui_round_btn(s, "+", z_plus_cb);
  lv_obj_align(plus, LV_ALIGN_CENTER, 125, -40);

  // Schrittweite 1 / 10 / 100
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 30);
  for (int i = 0; i < 3; i++) {
    char b[8];
    snprintf(b, sizeof(b), "%d", STEPS[i]);
    z_step_btns[i] = ui_btn(row, b, BTN_NORMAL);
    lv_obj_set_height(z_step_btns[i], 44);
    lv_obj_set_style_pad_hor(z_step_btns[i], 16, 0);
    lv_obj_add_event_cb(z_step_btns[i], z_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)STEPS[i]);
  }

  lv_obj_t *start = ui_btn(s, "Start", BTN_PRIMARY);
  lv_obj_align(start, LV_ALIGN_CENTER, 0, 105);
  lv_obj_add_event_cb(start, z_start_cb, LV_EVENT_CLICKED, NULL);

  z_show();
  return s;
}

// ------------------------------------------------------------
//  Schritt 2: Annäherung mit Ring
// ------------------------------------------------------------
static lv_obj_t *za_ring, *za_weight, *za_rest, *za_hint, *za_btns, *za_toast;
static lv_timer_t *za_timer;
static int za_prev_state;
static int za_prev_done;
static bool za_logged;
static lv_obj_t *za_tara;  // Ergebnis dieses Ziels schon im Protokoll?

static void za_new_cb(lv_event_t *e) {
  ui_switch_page(page_ziel_create());
}


static uint32_t za_neg_since = 0;

static void za_timer_cb(lv_timer_t *t) {
  // Topf abgenommen, Waage steht im Minus: nach 3 s wieder auf 0 (wie auf der Wiegeseite).
  // Danach zählt es als neuer Durchgang, das nächste Ergebnis kommt wieder ins Protokoll.
  if (ui_neg_tare_due(&za_neg_since, true)) {
    scale_tare();
    sound_play(SND_TARA);
    sound_parking_reset();
    ui_toast_show(za_toast, "Tara gesetzt", C_ACCENT);
    za_logged = false;
  }
  float g = scale_net();
  float rest = ziel_g - g;

  int v = (int)(g / ziel_g * 1000.0f);
  if (v < 0) v = 0;
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(za_ring) != v) lv_arc_set_value(za_ring, v);

  char b[32];
  ui_fmt_weight(b, sizeof(b), g);
  ui_label_update(za_weight, b);

  // 0 = unter Ziel, 1 = erreicht, 2 = zu viel
  int state = rest > ZIEL_TOLERANZ_G ? 0 : (rest >= -ZIEL_TOLERANZ_G ? 1 : 2);
  if (state == 0) snprintf(b, sizeof(b), T("noch %d g"), (int)(rest + 0.5f));
  else if (state == 1) snprintf(b, sizeof(b), T("Ziel erreicht"));
  else snprintf(b, sizeof(b), T("%d g zu viel"), (int)(-rest + 0.5f));
  ui_label_update(za_rest, b);

  if (state != za_prev_state) {
    za_prev_state = state;
    lv_color_t c = state == 2 ? C_DANGER : C_ACCENT;
    lv_obj_set_style_arc_color(za_ring, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(za_rest, c, 0);
  }
  sound_parking(g, (float)ziel_g, ZIEL_TOLERANZ_G);  // Parkpiepser

  // Ziel erreicht und ruhig -> "Neues Ziel" / "Speichern" statt Hinweis
  int done = (state == 1 && scale_stable()) ? 1 : 0;
  if (done != za_prev_done) {
    za_prev_done = done;
    if (done) {
      lv_obj_clear_flag(za_btns, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(za_hint, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(za_tara, LV_OBJ_FLAG_HIDDEN);
      if (!za_logged) {  // Ergebnis automatisch mitschreiben
        za_logged = true;
        char note[24];
        snprintf(note, sizeof(note), T("Ziel %d g"), ziel_g);
        if (log_add(g, note)) ui_toast_show(za_toast, "Im Protokoll gespeichert", C_ACCENT);
      }
    } else {
      lv_obj_add_flag(za_btns, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(za_hint, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(za_tara, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static lv_obj_t *page_ziel_active_create() {
  lv_obj_t *s = ui_screen_create();
  za_ring = ui_ring(s, 396);

  char b[24];
  snprintf(b, sizeof(b), T("Ziel %d g"), ziel_g);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);

  za_weight = ui_label(s, "0,0", &font_sg_80, C_TEXT);
  lv_obj_align(za_weight, LV_ALIGN_CENTER, 0, -20);

  za_rest = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(za_rest, LV_ALIGN_CENTER, 0, 60);

  za_hint = ui_label(s, "Nach rechts wischen: beenden", &font_sg_14, C_FAINT);
  lv_obj_align(za_hint, LV_ALIGN_CENTER, 0, 152);

  lv_obj_t *tara = ui_btn(s, "Tara", BTN_NORMAL);
  lv_obj_set_style_pad_hor(tara, 22, 0);
  lv_obj_align(tara, LV_ALIGN_CENTER, 0, 112);
  lv_obj_add_event_cb(tara, [](lv_event_t *e) {
    scale_tare();
    sound_play(SND_TARA);
  }, LV_EVENT_CLICKED, NULL);
  za_tara = tara;

  za_btns = ui_box(s);
  lv_obj_set_size(za_btns, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(za_btns, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(za_btns, 10, 0);
  lv_obj_align(za_btns, LV_ALIGN_CENTER, 0, 112);
  lv_obj_t *nb = ui_btn(za_btns, "Neues Ziel", BTN_PRIMARY);
  lv_obj_add_event_cb(nb, za_new_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(za_btns, LV_OBJ_FLAG_HIDDEN);

  za_toast = ui_toast_create(s);
  lv_obj_align(za_toast, LV_ALIGN_CENTER, 0, 158);

  za_prev_state = -1;
  za_prev_done = -1;
  za_logged = false;
  za_neg_since = 0;
  za_timer = ui_page_timer(s, za_timer_cb, 100);
  za_timer_cb(NULL);
  return s;
}
