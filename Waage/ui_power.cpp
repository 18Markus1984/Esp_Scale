#include "ui_power.h"
#include "ui_theme.h"
#include "hal.h"
#include "scale.h"
#include "sound.h"
#include "settings.h"
#include "web.h"
#include "batt.h"
#include "tools.h"
#include "config.h"
#include <stdio.h>
#include <math.h>

#define SHORT_MAX_MS 800     // bis hier: kurzer Druck
#define OFF_HOLD_MS 3000     // so lange halten zum Ausschalten
#define BRIGHT_ON 60         // Helligkeit im Betrieb (%)
#define BRIGHT_STANDBY 8     // Helligkeit der Standby-Uhr (%)
#define WAKE_WEIGHT_G 5.0f   // so viel Gewichtsänderung weckt auf
#define OFF_WARN_S 15        // so lange vor dem Auto-Aus warnen
#define ACTIVE_WEIGHT_G 2.0f // Gewichtsänderung zählt als Bedienung

typedef enum { P_RUN, P_STANDBY, P_OFF } pstate_t;

static pstate_t s_state = P_RUN;
static uint32_t s_press_ms = 0;
static bool s_ignore_press = false;   // Taste noch vom Einschalten gedrückt
static float s_standby_ref_g = 0;
static uint32_t s_off_at = 0;
static bool s_released = false;       // Akku schon freigegeben?

static lv_obj_t *o_standby, *o_clock, *o_date;
static lv_obj_t *o_hold, *o_hold_ring;
static lv_obj_t *o_off, *o_off_info;
static lv_obj_t *o_warn, *o_warn_ring, *o_warn_secs;
// feste Texte der Überlagerungen, für den Sprachwechsel (Reihenfolge wie O_TXT)
static lv_obj_t *o_txt[6];
static const char *const O_TXT[6] = { "Antippen oder Taste: aufwecken", "Ausschalten", "Taste weiter gedrückt halten",
                                      "Aus", "Schaltet sich aus", "Antippen zum Abbrechen" };
static float s_act_ref_g = 0;         // Gewicht bei der letzten "Bedienung"
static uint32_t s_act_weight_ms = 0;  // Zeit der letzten Gewichtsänderung

// ------------------------------------------------------------
static lv_obj_t *overlay() {
  lv_obj_t *o = ui_box(lv_layer_top());
  lv_obj_set_size(o, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_set_style_bg_color(o, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);  // Berührungen nicht durchlassen
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  return o;
}

static void show(lv_obj_t *o, bool on) {
  if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void standby_update() {
  int y, mo, d, h, mi, s;
  char b[24];
  bool ok = hal_now(&y, &mo, &d, &h, &mi, &s);
  snprintf(b, sizeof(b), "%02d:%02d", h, mi);
  ui_label_update(o_clock, b);
  if (ok) snprintf(b, sizeof(b), "%02d.%02d.%04d", d, mo, y);
  else b[0] = 0;
  ui_label_update(o_date, b);
}

static void enter_standby() {
  s_state = P_STANDBY;
  s_standby_ref_g = scale_net();
  standby_update();
  show(o_standby, true);
  hal_backlight(BRIGHT_STANDBY);
}

static void wake() {
  s_state = P_RUN;
  show(o_standby, false);
  hal_backlight(BRIGHT_ON);
  lv_disp_trig_activity(NULL);
}

static void standby_touch_cb(lv_event_t *e) {
  wake();
}

static void warn_touch_cb(lv_event_t *e) {  // Antippen bricht das Auto-Aus ab
  show(o_warn, false);
  lv_disp_trig_activity(NULL);
  s_act_weight_ms = hal_millis();
  if (s_state == P_STANDBY) wake();
}

static void power_off() {
  s_state = P_OFF;
  show(o_warn, false);
  show(o_hold, false);
  show(o_standby, false);
  show(o_off, true);
  sound_play(SND_OVERLOAD);
  s_off_at = hal_millis();
  s_released = false;
}

// Aus dem Standby holen (z. B. wenn ein Timer klingelt)
void ui_power_wake() {
  if (s_state == P_STANDBY) wake();
}

// Nach einem Sprachwechsel die festen Texte neu setzen
void ui_power_lang_changed() {
  for (int i = 0; i < 6; i++)
    if (o_txt[i]) lv_label_set_text(o_txt[i], O_TXT[i]);
}

// ------------------------------------------------------------
void ui_power_init() {
  // Standby: gedimmte Uhr
  o_standby = overlay();
  o_clock = ui_label(o_standby, "--:--", &font_sg_80, C_FAINT);
  lv_obj_align(o_clock, LV_ALIGN_CENTER, 0, -18);
  o_date = ui_label(o_standby, "", &font_sg_18, C_FAINT);
  lv_obj_align(o_date, LV_ALIGN_CENTER, 0, 46);
  lv_obj_t *h = ui_label(o_standby, "Antippen oder Taste: aufwecken", &font_sg_14, lv_color_hex(0x45443E));
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 96);
  o_txt[0] = h;
  lv_obj_add_event_cb(o_standby, standby_touch_cb, LV_EVENT_CLICKED, NULL);

  // Halten zum Ausschalten
  o_hold = overlay();
  o_hold_ring = ui_ring(o_hold, 396);
  lv_obj_set_style_arc_color(o_hold_ring, C_DANGER, LV_PART_INDICATOR);
  lv_obj_t *t = ui_label(o_hold, "Ausschalten", &font_sg_34, C_TEXT);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -14);
  o_txt[1] = t;
  lv_obj_t *t2 = ui_label(o_hold, "Taste weiter gedrückt halten", &font_sg_14, C_MUTED);
  lv_obj_align(t2, LV_ALIGN_CENTER, 0, 30);
  o_txt[2] = t2;

  // Aus
  o_off = overlay();
  lv_obj_t *a = ui_label(o_off, "Aus", &font_sg_34, C_MUTED);
  lv_obj_align(a, LV_ALIGN_CENTER, 0, -20);
  o_txt[3] = a;
  o_off_info = ui_label(o_off, "", &font_sg_14, C_FAINT);
  lv_obj_align(o_off_info, LV_ALIGN_CENTER, 0, 30);

  // Warnung vor dem automatischen Ausschalten
  o_warn = overlay();
  o_warn_ring = ui_ring(o_warn, 396);
  lv_obj_set_style_arc_color(o_warn_ring, C_WARN, LV_PART_INDICATOR);
  lv_obj_t *w1 = ui_label(o_warn, "Schaltet sich aus", &font_sg_24, C_TEXT);
  lv_obj_align(w1, LV_ALIGN_CENTER, 0, -70);
  o_txt[4] = w1;
  o_warn_secs = ui_label(o_warn, "", &font_sg_80, C_WARN);
  lv_obj_align(o_warn_secs, LV_ALIGN_CENTER, 0, 0);
  lv_obj_t *w2 = ui_label(o_warn, "Antippen zum Abbrechen", &font_sg_14, C_MUTED);
  lv_obj_align(w2, LV_ALIGN_CENTER, 0, 70);
  o_txt[5] = w2;
  lv_obj_add_event_cb(o_warn, warn_touch_cb, LV_EVENT_CLICKED, NULL);

  // Wurde die Waage mit der Taste eingeschaltet, ist sie evtl. noch gedrückt
  s_ignore_press = hal_pwr_pressed();
  hal_backlight(BRIGHT_ON);
}

bool ui_power_standby() {
  return s_state == P_STANDBY;
}

// Akku fast leer? Erst warnen, dann abschalten - auch während eines
// Entladetests, sonst wird die Zelle tiefentladen.
static void battery_guard() {
  static uint32_t low_since = 0, warn_at = 0;
  float v = hal_battery_volts();
  bat_state_t st = hal_battery_state();
  if (v < 2.5f || st == BAT_CHARGING || st == BAT_FULL) {  // am Kabel nicht abschalten
    low_since = 0;
    return;
  }
  uint32_t now = hal_millis();
  if (v > BAT_WARN_V) {
    low_since = 0;
    return;
  }
  if (now - warn_at > 60000) {  // einmal pro Minute hörbar warnen
    warn_at = now;
    sound_play(SND_WARN);
  }
  if (v > BAT_CUTOFF_V) {
    low_since = 0;
    return;
  }
  if (low_since == 0) low_since = now;
  if (now - low_since < 20000) return;  // 20 s halten, Messrauschen ignorieren
  printf("Akku leer (%.2f V): Waage schaltet ab\r\n", v);
  batt_test_stop();
  power_off();
}

void ui_power_tick() {
  uint32_t now = hal_millis();
  bool pressed = hal_pwr_pressed();
  battery_guard();

  // Ausgeschaltet: Akku freigeben. Läuft die Waage danach noch, hängt sie am USB.
  if (s_state == P_OFF) {
    if (!s_released && now - s_off_at > 400) {
      hal_power_hold(false);
      s_released = true;
    }
    if (s_released && now - s_off_at > 1500) {
      lv_label_set_text(o_off_info, "Versorgung über USB\nTaste drücken: einschalten");
      hal_backlight(now - s_off_at > 8000 ? 0 : BRIGHT_STANDBY);
    }
  }

  if (s_ignore_press) {
    if (!pressed) s_ignore_press = false;
    return;
  }

  if (pressed) {
    s_press_ms += 100;
    if (s_state == P_OFF) return;  // Einschalten erst beim Loslassen
    if (s_press_ms > SHORT_MAX_MS) {
      if (s_state == P_STANDBY) wake();
      show(o_hold, true);
      int v = (int)((s_press_ms - SHORT_MAX_MS) * 1000 / (OFF_HOLD_MS - SHORT_MAX_MS));
      lv_arc_set_value(o_hold_ring, v > 1000 ? 1000 : v);
      if (s_press_ms >= OFF_HOLD_MS) {
        power_off();
        s_ignore_press = true;  // Loslassen danach nicht als "Einschalten" werten
        s_press_ms = 0;
      }
    }
    return;
  }

  // Taste losgelassen
  if (s_press_ms > 0) {
    bool short_press = s_press_ms <= SHORT_MAX_MS;
    s_press_ms = 0;
    show(o_hold, false);
    if (s_state == P_OFF) {
      hal_restart();  // am USB: neu starten = einschalten
    } else if (short_press) {
      if (s_state == P_RUN) enter_standby();
      else wake();
    }
  }

  // ---- Automatisch ausschalten ----
  // Bedienung = Berühren, PWR-Taste oder Gewichtsänderung. Weboberfläche
  // mit Zugriffen hält die Waage ebenfalls an.
  if (s_state != P_OFF) {
    float g = scale_net();
    if (fabsf(g - s_act_ref_g) > ACTIVE_WEIGHT_G) {
      s_act_ref_g = g;
      s_act_weight_ms = now;
    }
    if (pressed || (web_running() && web_seconds_left() > 0)) s_act_weight_ms = now;
    // Während eines Entladetests nicht abschalten, sonst endet der Test
    // nach 30 Minuten ohne Bedienung und die Messung ist wertlos.
    // Ebenso nicht bei laufendem Küchentimer oder Langzeitmessung.
    if (g_set.auto_off_min > 0 && !batt_test_running() && !tools_keep_awake()) {
      uint32_t limit = (uint32_t)g_set.auto_off_min * 60000UL;
      uint32_t touch = lv_disp_get_inactive_time(NULL);
      uint32_t weight = now - s_act_weight_ms;
      uint32_t idle = touch < weight ? touch : weight;
      if (idle + OFF_WARN_S * 1000UL >= limit) {
        uint32_t left = idle >= limit ? 0 : (limit - idle + 999) / 1000;
        if (lv_obj_has_flag(o_warn, LV_OBJ_FLAG_HIDDEN)) {
          hal_backlight(BRIGHT_ON);  // auch aus dem Standby heraus sichtbar machen
          show(o_warn, true);
          sound_play(SND_WARN);
        }
        char b[8];
        snprintf(b, sizeof(b), "%lu", (unsigned long)left);
        ui_label_update(o_warn_secs, b);
        lv_arc_set_value(o_warn_ring, (int)(left * 1000 / OFF_WARN_S));
        if (left == 0) power_off();
      } else if (!lv_obj_has_flag(o_warn, LV_OBJ_FLAG_HIDDEN)) {
        show(o_warn, false);  // wieder bedient
        hal_backlight(s_state == P_STANDBY ? BRIGHT_STANDBY : BRIGHT_ON);
      }
    }
  }

  // Standby: Uhr aktualisieren, bei Gewichtsänderung aufwachen
  if (s_state == P_STANDBY) {
    standby_update();
    if (fabsf(scale_net() - s_standby_ref_g) > WAKE_WEIGHT_G) wake();
  }
}
