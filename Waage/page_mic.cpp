// ============================================================
//  System -> Mikrofon
//  Zeigt ohne Zahlenwerte, ob das Mikrofon etwas hört: Der Ring
//  schlägt mit dem Schall aus, darunter steht der Zustand. Ist die
//  Spracherkennung eingeschaltet (USE_VOICE), steht dort außerdem,
//  ob das Weckwort erkannt wurde und welcher Befehl verstanden wurde.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "mic.h"
#include "voice.h"
#include "config.h"
#include "hal.h"
#include <stdio.h>
#include <math.h>

static lv_obj_t *m_ring, *m_icon, *m_state, *m_hint, *m_cmd;
static float m_loud = -90.0f;       // lautester Wert seit dem Öffnen
static uint32_t m_heard_at = 0;     // wann zuletzt etwas zu hören war

static void m_timer_cb(lv_timer_t *t) {
  float db = mic_level_db();
  float pk = mic_peak_db();
  uint32_t now = hal_millis();
  if (pk > m_loud) m_loud = pk;
  if (pk > -55.0f) m_heard_at = now;

  // Ring folgt dem Schall: -70 dB leer, -20 dB voll
  int v = (int)((db + 70.0f) / 50.0f * 1000.0f);
  if (v < 0) v = 0;
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(m_ring) != v) lv_arc_set_value(m_ring, v);

  // Läuft die Spracherkennung, gehört das Mikrofon ihr: dann keine Pegelmessung
  if (mic_exclusive() || voice_running() || voice_starting()) {
    lv_arc_set_value(m_ring, voice_awake() ? 1000 : 120);
    lv_color_t vc = voice_awake() ? C_ACCENT : C_TEXT2;
    lv_obj_set_style_arc_color(m_ring, vc, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(m_icon, vc, 0);
    lv_obj_set_style_text_color(m_state, C_TEXT, 0);
    ui_label_update(m_state, voice_starting() ? "startet …" : (voice_awake() ? "hört zu …" : "wartet"));
    ui_label_update(m_hint, voice_starting() ? "Sprachmodelle werden geladen" : "Weckwort: „Hi ESP“");
    char vb[64];
    if (voice_last()[0] && now - voice_last_ms() < 15000) snprintf(vb, sizeof(vb), "„%s“", voice_last());
    else snprintf(vb, sizeof(vb), T("noch kein Befehl"));
    ui_label_update(m_cmd, vb);
    lv_obj_set_style_text_color(m_cmd, voice_last()[0] ? C_ACCENT : C_FAINT, 0);
    return;
  }

  bool alive = mic_alive() && mic_raw_peak() > 0;
  bool heard = now - m_heard_at < 700;
  lv_color_t c = !alive ? C_DANGER : (heard ? C_ACCENT : C_TEXT2);
  lv_obj_set_style_arc_color(m_ring, c, LV_PART_INDICATOR);
  lv_obj_set_style_text_color(m_icon, c, 0);
  lv_obj_set_style_text_color(m_state, alive ? C_TEXT : C_DANGER, 0);

  if (!alive) {
    ui_label_update(m_state, "kein Signal");
    ui_label_update(m_hint, "Mikrofon antwortet nicht");
  } else if (voice_running() && voice_awake()) {
    ui_label_update(m_state, "hört zu …");
    ui_label_update(m_hint, "Befehl sagen");
  } else if (heard) {
    ui_label_update(m_state, "hört etwas");
    ui_label_update(m_hint, voice_running() ? "Weckwort: „Hi ESP“" : "sprich oder klatsche");
  } else {
    ui_label_update(m_state, m_loud > -50.0f ? "bereit" : "still");
    ui_label_update(m_hint, voice_running() ? "Weckwort: „Hi ESP“" : "sprich oder klatsche");
  }

  // Was zuletzt verstanden wurde
  char b[64];
  if (!voice_running()) {
    snprintf(b, sizeof(b), T("Spracherkennung aus"));
  } else if (voice_last()[0] && now - voice_last_ms() < 15000) {
    snprintf(b, sizeof(b), "„%s“", voice_last());
  } else {
    snprintf(b, sizeof(b), T("noch kein Befehl"));
  }
  ui_label_update(m_cmd, b);
  lv_obj_set_style_text_color(m_cmd, (voice_running() && voice_last()[0] && now - voice_last_ms() < 15000)
                                         ? C_ACCENT
                                         : C_FAINT,
                              0);
}

static void m_screen_del_cb(lv_event_t *e) {
  mic_listen(false);  // Pegelmessung wieder ausschalten
}

lv_obj_t *page_mic_create() {
  lv_obj_t *s = ui_screen_create();
  m_ring = ui_ring(s, 396);
  lv_obj_add_event_cb(s, m_screen_del_cb, LV_EVENT_DELETE, NULL);

  lv_obj_t *t = ui_label(s, "Mikrofon", &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  m_icon = ui_label(s, ICON_MIKRO, &font_icons_26, C_TEXT2);
  lv_obj_align(m_icon, LV_ALIGN_CENTER, 0, -70);

  m_state = ui_label(s, "still", &font_sg_34, C_TEXT);
  lv_obj_align(m_state, LV_ALIGN_CENTER, 0, -20);

  m_hint = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(m_hint, LV_ALIGN_CENTER, 0, 28);

  m_cmd = ui_label(s, "", &font_sg_18, C_FAINT);
  lv_obj_align(m_cmd, LV_ALIGN_CENTER, 0, 76);

  m_loud = -90.0f;
  m_heard_at = 0;
  mic_reset_claps();
  if (!mic_begin()) {
    ui_label_update(m_state, "nicht gefunden");
    lv_obj_set_style_text_color(m_state, C_DANGER, 0);
  }
  if (!mic_exclusive()) mic_listen(true);
  ui_page_timer(s, m_timer_cb, 100);
  m_timer_cb(NULL);
  return s;
}
