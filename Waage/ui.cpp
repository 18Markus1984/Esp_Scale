#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_pages.h"
#include "scale.h"
#include "hal.h"
#include "config.h"
#include "settings.h"
#include "storage.h"
#include "data.h"
#include "net.h"
#include "web.h"
#include "sound.h"
#include "ui_power.h"
#include "voice.h"
#include "batt.h"
#include "tools.h"
#include "update_online.h"
#include "refcheck.h"
#include "disp_rot.h"
#include <stdio.h>
#include <math.h>

// LVGL braucht für alle Seiten mehr Arbeitsspeicher als die 48 KB aus der
// Waveshare-Konfiguration. Anleitung dazu in der README.
#if LV_MEM_CUSTOM == 0 && LV_MEM_SIZE < (96U * 1024U)
#error "Bitte in libraries/lvgl/src/lv_conf.h  #define LV_MEM_CUSTOM 1  setzen (siehe README)"
#endif

// ============================================================
//  Startseite: Tileview mit Wiegen | Modi | System
// ============================================================
static lv_obj_t *scr_home, *h_tv, *h_tile0;
static lv_obj_t *h_wifi, *h_bat;
static lv_obj_t *h_ring, *h_status, *h_pot, *h_weight, *h_unit, *h_state, *h_potinfo, *h_liquid;
static void weight_show(const char *num, const char *unit);  // große Gewichtsanzeige (unten)
static lv_obj_t *h_btns, *h_btn_save, *h_btn_remove, *h_toast, *h_spk;
static bool h_boot_prev = false;
static int h_prev_state = -1;  // zuletzt gezeigter Zustand (misst/stabil/Überlast)

// Modi sind in drei Gruppen sortiert, sonst wird die Liste zu lang
static const ui_list_item_t MODI_GROUPS[] = {
  { "Küche", "Rezept · Timer · Tassen …", ICON_KUECHE },
  { "Werkstatt", "Spule · 2K · Messung …", ICON_WERKSTATT },
  { "Spiele", "Schätzen · Gießen · Teilen …", ICON_SPIELE },
};

static const ui_list_item_t GROUP_KUECHE[] = {
  { "Rezept", "Schritt für Schritt", ICON_REZEPT },
  { "Cocktail", "in ml mixen", ICON_COCKTAIL },
  { "Ziel", "Parkpiepser", ICON_ZIEL },
  { "Portionieren", "Teig gleich teilen", ICON_PORTION },
  { "Timer", "bis zu drei gleichzeitig", ICON_TIMER },
  { "Tassen & Löffel", "US-Rezepte umrechnen", ICON_TASSE },
};
static const ui_list_item_t GROUP_WERKSTATT[] = {
  { "Spule", "Restmeter Filament", ICON_SPULE },
  { "Zählen", "Stückzahl + Bluetooth", ICON_ZAEHLEN },
  { "Porto", "Versandklasse", ICON_PORTO },
  { "2K mischen", "Harz, Silikon, Topfzeit", ICON_MIX },
  { "Langzeit", "Gewicht über Stunden", ICON_LANGZEIT },
  { "Münzen", "zählen und Kassensturz", ICON_MUENZEN },
};
static const ui_list_item_t GROUP_SPIELE[] = {
  { "Schätzspiel", "Gewicht raten", ICON_SPIEL },
  { "Trinkspiel", "Schluck schätzen", ICON_TRINK },
  { "Blindgießen", "Menge ohne Anzeige", ICON_BLIND },
  { "Halbe-Halbe", "genau in der Mitte teilen", ICON_HALB },
};

static const ui_list_item_t SYSTEM_ITEMS[] = {
  { "Protokoll", "Heutige Wägungen", ICON_PROTOKOLL },
  { "Töpfe", "Verwalten", ICON_TOEPFE },
  { "Wasserwaage", "Lage prüfen", ICON_LIBELLE },
  { "Akku", "Verlauf und Restzeit", ICON_AKKU },
  { "Setup", "Einstellungen", ICON_SETUP },
};

// Welche Seite zu welchem Listeneintrag gehört.
// NULL = noch nicht gebaut -> Platzhalter wird angezeigt.
// Reihenfolge muss zu MODI_ITEMS / SYSTEM_ITEMS passen!
static const page_create_fn PAGES_KUECHE[] = { page_rezept_create,  page_cocktail_create, page_ziel_create,
                                                page_portion_create, page_timer_create,    page_tassen_create };
static const page_create_fn PAGES_WERKSTATT[] = { page_spule_create, page_zaehlen_create,  page_porto_create,
                                                   page_mix_create,   page_langzeit_create, page_muenzen_create };
static const page_create_fn PAGES_SPIELE[] = { page_spiel_create, page_trink_create, page_blind_create,
                                               page_halb_create };

static const ui_list_item_t *const GROUP_ITEMS[] = { GROUP_KUECHE, GROUP_WERKSTATT, GROUP_SPIELE };
static const page_create_fn *const GROUP_PAGES[] = { PAGES_KUECHE, PAGES_WERKSTATT, PAGES_SPIELE };
static const int GROUP_COUNT[] = { 6, 6, 4 };

static const page_create_fn SYSTEM_PAGES[] = {
  page_protokoll_create,  // Protokoll
  page_toepfe_create,     // Töpfe
  page_level_create,      // Wasserwaage
  page_akku_create,       // Akku
  page_setup_create,      // Setup
};

// ============================================================
//  Topf-Zustand auf der Wiegeseite
// ============================================================
#define POT_NONE 0
#define POT_AUTO 1   // automatisch erkannt, Tara auf Topf gesetzt
#define POT_LIST 2   // aus der Liste gewählt, gespeichertes Gewicht abgezogen

#define EMPTY_G 3.0f       // darunter gilt die Waage als leer
#define MIN_POT_G 20.0f    // kleinere Gewichte werden nicht als Topf geprüft
#define EMPTY_ARM_MS 500   // so lange leer, bevor wieder erkannt wird

static int s_pot_mode = POT_NONE;
static int s_pot_idx = -1;
static float s_base_tare = 0;   // Tara der leeren Waage
static bool s_armed = false;    // bereit für die nächste Erkennung
static uint32_t s_empty_since = 0;

// Countdown-Overlay
static lv_obj_t *cd_box, *cd_ring, *cd_name, *cd_sub, *cd_secs, *cd_hint;
static bool cd_active = false;
static uint32_t cd_start = 0;
static int cd_idx = -1;

static void set_pot_mode(int mode, int idx) {
  s_pot_mode = mode;
  s_pot_idx = idx;
  char b[64];
  if (mode == POT_NONE) {
    lv_obj_add_flag(h_pot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(h_potinfo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(h_btns, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(h_btn_remove, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_color_t c = lv_color_hex(pot_color_hex(g_pots[idx].color));
  if (mode == POT_AUTO) {
    ui_chip_set(h_pot, g_pots[idx].name, c, false);
    lv_obj_add_flag(h_potinfo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(h_btns, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(h_btn_remove, LV_OBJ_FLAG_HIDDEN);
  } else {
    snprintf(b, sizeof(b), T("Netto · %s abgezogen"), g_pots[idx].name);
    ui_chip_set(h_pot, b, c, false);
    lv_obj_clear_flag(h_potinfo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(h_btns, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(h_btn_remove, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_clear_flag(h_pot, LV_OBJ_FLAG_HIDDEN);
}

static void pot_clear() {
  if (s_pot_mode != POT_NONE) scale_set_tare(s_base_tare);
  set_pot_mode(POT_NONE, -1);
}

void ui_pot_subtract(int idx) {
  if (idx < 0 || idx >= g_pot_count) return;
  if (s_pot_mode == POT_NONE) s_base_tare = scale_get_tare();
  scale_set_tare(s_base_tare + g_pots[idx].grams);
  set_pot_mode(POT_LIST, idx);
  s_armed = false;
  lv_obj_set_tile_id(h_tv, 0, 0, LV_ANIM_OFF);
  ui_go_home();
}

// Sucht rekursiv einen Knopf mit passender Beschriftung und klickt ihn
static lv_obj_t *find_btn(lv_obj_t *obj, const char *const labels[], int count) {
  if (lv_obj_check_type(obj, &lv_label_class)) {
    const char *txt = lv_label_get_text(obj);
    for (int i = 0; i < count; i++)
      if (strcmp(txt, labels[i]) == 0 || strcmp(txt, T(labels[i])) == 0) {  // auch auf Englisch
        lv_obj_t *p = lv_obj_get_parent(obj);
        if (p && lv_obj_has_flag(p, LV_OBJ_FLAG_CLICKABLE) && !lv_obj_has_state(p, LV_STATE_DISABLED)) return p;
      }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++) {
    lv_obj_t *r = find_btn(lv_obj_get_child(obj, i), labels, count);
    if (r) return r;
  }
  return NULL;
}

bool ui_click_button(const char *const labels[], int count) {
  lv_obj_t *b = find_btn(lv_scr_act(), labels, count);
  if (!b) return false;
  lv_event_send(b, LV_EVENT_CLICKED, NULL);
  return true;
}

const char *ui_active_pot() {
  return (s_pot_mode != POT_NONE && s_pot_idx >= 0) ? g_pots[s_pot_idx].name : "";
}

void ui_pots_changed() {
  pot_clear();  // Liste hat sich geändert -> aktiven Topf sicherheitshalber lösen
}

// ---------- Toast ----------
static void show_toast(const char *txt, lv_color_t color) {
  ui_toast_show(h_toast, txt, color);
}

// ---------- Buttons ----------
static void tara_cb(lv_event_t *e) {
  scale_tare();
  sound_play(SND_TARA);
  if (s_pot_mode != POT_NONE) set_pot_mode(POT_NONE, -1);
  show_toast("Tara gesetzt", C_ACCENT);
}

// Ansage-Zustand der Wiegeseite (siehe speak_check)
static uint32_t sp_stable_since = 0;
static bool sp_done = false;  // Gewicht dieser Auflage schon angesagt/eingereiht

// Nach dem Speichern: Wurde das Gewicht gerade schon angesagt, nur noch
// "gespeichert" – sonst Gewicht + "gespeichert" in einem Satz.
static void speak_saved(float g) {
  if (g_set.speak && !sp_done) {
    sound_speak_weight_word(g, g_set.unit, "gespeichert");
    sp_done = true;  // speak_check sagt dieselbe Auflage nicht noch einmal an
  } else {
    sound_speak_word("gespeichert");
  }
}

static void save_cb(lv_event_t *e) {
  if (!scale_stable()) {
    sound_play(SND_WARN);
    show_toast("Noch nicht stabil", C_WARN);
    return;
  }
  const char *note = s_pot_mode != POT_NONE ? g_pots[s_pot_idx].name : "";
  float g = scale_net(), pu;
  if (g_set.precise) scale_precise(&g, &pu, NULL);  // gemittelten Wert speichern
  int r = log_add(g, note);
  char buf[48];
  int hh, mm;
  if (r == 0) sound_play(SND_WARN);
  else sound_play_tone(SND_SAVE);  // Ansage folgt unten mit Gewicht + "gespeichert"
  if (r == 0) {
    show_toast("Keine SD-Karte", C_WARN);
  } else if (r < 0) {
    show_toast("Gespeichert · Uhr nicht gestellt", C_WARN);
    speak_saved(g);
  } else {
    if (hal_time(&hh, &mm)) snprintf(buf, sizeof(buf), T("Gespeichert · %02d:%02d"), hh, mm);
    else snprintf(buf, sizeof(buf), T("Gespeichert"));
    show_toast(buf, C_ACCENT);
    speak_saved(g);
  }
}

static void remove_pot_cb(lv_event_t *e) {
  pot_clear();
}

// ---------- Countdown ----------
static void cd_show(int idx, float grams) {
  cd_active = true;
  cd_idx = idx;
  cd_start = hal_millis();
  char b[48], w[16];
  lv_label_set_text(cd_name, g_pots[idx].name);
  ui_fmt_weight(w, sizeof(w), grams);
  snprintf(b, sizeof(b), T("erkannt · %s g"), w);
  lv_label_set_text(cd_sub, b);
  lv_obj_clear_flag(cd_box, LV_OBJ_FLAG_HIDDEN);
}

static void cd_hide() {
  cd_active = false;
  lv_obj_add_flag(cd_box, LV_OBJ_FLAG_HIDDEN);
}

static void cd_cancel_cb(lv_event_t *e) {
  cd_hide();  // s_armed bleibt false, bis die Waage wieder leer ist
}

static void cd_update() {
  uint32_t total = (uint32_t)g_set.countdown_s * 1000;
  uint32_t el = hal_millis() - cd_start;
  if (scale_net() < MIN_POT_G) {  // Topf wieder weggenommen
    cd_hide();
    return;
  }
  if (el >= total) {
    cd_hide();
    s_base_tare = scale_get_tare();
    scale_tare();
    set_pot_mode(POT_AUTO, cd_idx);
    sound_play(SND_POT);
    show_toast("Tara gesetzt", C_ACCENT);
    return;
  }
  int left = (int)((total - el + 999) / 1000);
  static int prev_left = -1;
  if (left != prev_left) {  // jede Sekunde ein Tick
    prev_left = left;
    sound_play(SND_TICK);
  }
  char b[24];
  snprintf(b, sizeof(b), "%d", left);
  ui_label_update(cd_secs, b);
  snprintf(b, sizeof(b), T("Tara in %d s"), left);
  ui_label_update(cd_hint, b);
  lv_arc_set_value(cd_ring, (int)(1000 - el * 1000 / total));
}

// Erkennung: Waage war leer -> Gewicht liegt ruhig -> passt zu einem Topf?
static void pot_detect() {
  float raw = scale_gross();

  // Topf wieder abgenommen?
  if (s_pot_mode != POT_NONE && raw - s_base_tare < EMPTY_G) {
    pot_clear();
  }

  float net = scale_net();
  uint32_t now = hal_millis();
  if (fabsf(net) < EMPTY_G) {
    if (s_empty_since == 0) s_empty_since = now;
    if (now - s_empty_since > EMPTY_ARM_MS) s_armed = true;
    return;
  }
  s_empty_since = 0;

  if (!g_set.autotara || !s_armed || s_pot_mode != POT_NONE || cd_active) return;
  if (!scale_stable() || net < MIN_POT_G) return;
  s_armed = false;  // pro Auflegen nur einmal prüfen
  int idx = pot_match(net, (float)g_set.tol_g, -1);
  if (idx >= 0) cd_show(idx, net);
}

// ---------- Launcher ----------
static int s_group = 0;
static lv_obj_t *group_page_create();
static page_create_fn s_back = NULL;  // Ziel der Zurück-Geste (NULL = Wiegeseite)

static void group_pick_cb(int index) {
  s_back = group_page_create;  // Wischen nach rechts führt zurück in die Gruppe
  page_create_fn fn = GROUP_PAGES[s_group][index];
  ui_switch_page(fn ? fn() : page_placeholder_create(GROUP_ITEMS[s_group][index].title));
}

static lv_obj_t *group_page_create() {
  lv_obj_t *s = ui_screen_create();
  ui_curved_list(s, GROUP_ITEMS[s_group], GROUP_COUNT[s_group], group_pick_cb);
  lv_obj_t *t = ui_label(s, MODI_GROUPS[s_group].title, &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return s;
}

static void modi_cb(int index) {
  s_group = index;
  s_back = NULL;
  ui_open_page(group_page_create());
}

static void system_cb(int index) {
  page_create_fn fn = SYSTEM_PAGES[index];
  ui_open_page(fn ? fn() : page_placeholder_create(SYSTEM_ITEMS[index].title));
}

// ---------- Auto-Speichern ----------
// Speichert einmal pro Auflegen, sobald das Gewicht ruhig liegt.
// Erst nach dem Abnehmen (Waage wieder leer) wird erneut gespeichert.
static uint32_t as_stable_since = 0;
static bool as_done = false;

static void autosave_check() {
  float net = scale_net();
  if (fabsf(net) < EMPTY_G) {  // leer -> bereit für die nächste Wägung
    as_done = false;
    as_stable_since = 0;
    return;
  }
  if (!g_set.autosave || as_done || cd_active || net < AUTOSAVE_MIN_G || scale_overload()) return;
  if (!scale_stable()) {
    as_stable_since = 0;
    return;
  }
  // gerade aufgelegter leerer Topf? Den nicht speichern, die Erkennung übernimmt
  if (g_set.autotara && s_pot_mode == POT_NONE && pot_match(net, (float)g_set.tol_g, -1) >= 0) return;

  uint32_t now = hal_millis();
  if (as_stable_since == 0) as_stable_since = now;
  if (now - as_stable_since < AUTOSAVE_STABLE_MS) return;

  as_done = true;
  const char *note = s_pot_mode != POT_NONE ? g_pots[s_pot_idx].name : "";
  int r = log_add(net, note);
  sound_play(r == 0 ? SND_WARN : SND_SAVE);
  if (r == 0) show_toast("Keine SD-Karte", C_WARN);
  else show_toast("Automatisch gespeichert", C_ACCENT);
}

// ---------- Gewicht ansagen ----------
// Einmal pro Auflegen, sobald der Wert ruhig liegt.
// Symbol und Farbe des Lautsprecher-Knopfs nachführen (alle 100 ms)
static void speaker_update() {
  if (!h_spk) return;
  static int8_t shown = -2;  // -1 = versteckt
  static bool was_talking = false;
  bool avail = sound_voice_available() && g_set.volume > 0;
  int mode = avail ? sound_speak_mode() : -1;
  bool talking = avail && sound_speaking();
  if (mode == shown && talking == was_talking) return;
  shown = (int8_t)mode;
  was_talking = talking;
  if (mode < 0) {
    lv_obj_add_flag(h_spk, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(h_spk, LV_OBJ_FLAG_HIDDEN);
  static const char *const IC[4] = { ICON_TON, ICON_ZAEHLEN, ICON_SPRUECHE, ICON_TON_AUS };
  lv_obj_t *ic = lv_obj_get_child(h_spk, 0);
  lv_label_set_text(ic, IC[mode]);
  lv_obj_set_style_text_color(ic, talking ? C_ACCENT : (mode == SPEAK_OFF ? C_FAINT : C_MUTED), 0);
}

static void speaker_cb(lv_event_t *e) {
  char b[48];
  if (sound_speaking()) {
    sound_speak_stop();
    show_toast("Ansage gestoppt", C_MUTED);
  } else {
    int mode = (sound_speak_mode() + 1) % 4;
    sound_speak_mode_set(mode);
    settings_save();
    sound_play_tone(SND_CLICK);
    snprintf(b, sizeof(b), T("Ansage: %s"), T(sound_speak_mode_name(mode)));
    show_toast(b, C_ACCENT);
  }
  speaker_update();
}

static void speak_check() {
  speaker_update();
  float net = scale_net();
  static bool sp_was_on = false;   // es lag etwas, das angesagt wurde
  static uint32_t sp_empty_since = 0;
  if (fabsf(net) < EMPTY_G) {
    if (sp_done) sp_was_on = true;
    sp_done = false;
    sp_stable_since = 0;
    // Null nur durch Tara (Topf/Ware steht noch drauf): nicht "Waage leer"
    if (fabsf(scale_get_tare()) > 20.0f) sp_was_on = false;
    // nach dem Abnehmen einmal "Waage leer" (wenn die Sprüche an sind)
    if (sp_was_on && g_set.speak_ev && scale_stable()) {
      if (!sp_empty_since) sp_empty_since = hal_millis();
      if (hal_millis() - sp_empty_since > 800) {
        sound_speak_word("waage_leer");
        sp_was_on = false;
        sp_empty_since = 0;
      }
    } else {
      sp_empty_since = 0;
    }
    return;
  }
  sp_empty_since = 0;
  if (!g_set.speak || sp_done || cd_active || fabsf(net) < 2.0f) return;
  if (!scale_stable()) {
    sp_stable_since = 0;
    return;
  }
  uint32_t now = hal_millis();
  if (sp_stable_since == 0) sp_stable_since = now;
  if (now - sp_stable_since < 1500) return;
  sp_done = true;
  sound_speak_weight(net, g_set.unit);
}

// ---------- Zurück zur Wiegeseite nach Inaktivität ----------
// Als Bedienung zählt Berühren und auch eine Gewichtsänderung,
// damit z. B. ein laufendes Rezept beim Abwiegen offen bleibt.
static bool s_start_done = false;  // Lagecheck beim Einschalten abgeschlossen
static float idle_ref_g = 0;
static uint32_t idle_weight_ms = 0;

static void idle_check() {
  uint32_t now = hal_millis();
  float net = scale_net();
  if (fabsf(net - idle_ref_g) > 2.0f) {
    idle_ref_g = net;
    idle_weight_ms = now;
  }
  if (g_set.idle_min <= 0 || !s_start_done) return;
  uint32_t limit = (uint32_t)g_set.idle_min * 60000UL;
  uint32_t touch_idle = lv_disp_get_inactive_time(NULL);
  if (touch_idle < limit || now - idle_weight_ms < limit) return;

  bool home_weigh = lv_scr_act() == scr_home && lv_tileview_get_tile_act(h_tv) == h_tile0;
  if (home_weigh) return;
  lv_obj_set_tile_id(h_tv, 0, 0, LV_ANIM_OFF);
  if (lv_scr_act() != scr_home) ui_go_home();
  lv_disp_trig_activity(NULL);  // Zähler zurücksetzen
}

// ---------- Laufende Aktualisierung (alle 100 ms) ----------
// Finger auf dem Display oder Seitenwechsel per Wischen in Bewegung?
static bool touch_busy() {
  lv_indev_t *in = lv_indev_get_next(NULL);
  if (in && in->proc.state == LV_INDEV_STATE_PRESSED) return true;
  return h_tv && lv_obj_is_scrolling(h_tv);
}

// ------------------------------------------------------------
//  Touch-Filter
//  Der Touch-Controller meldet beim Wischen manchmal kurz "losgelassen"
//  und nach dem Loslassen gelegentlich noch einen kurzen Geister-Druck.
//  Folgen ohne Filter: Seiten schnappen beim Wischen zurück, Tasten
//  zählen doppelt, und nach einem Seitenwechsel wird auf der neuen Seite
//  an derselben Stelle etwas "gedrückt".
//  Der Filter sitzt zwischen Treiber und LVGL:
//   - ein Druck zählt erst, wenn er 2 Abfragen hintereinander da ist
//   - kurze Aussetzer (< 60 ms) während eines Drucks werden überbrückt,
//     dabei läuft der Finger in seiner Richtung weiter (Schwung bleibt)
//   - nach jedem Seitenwechsel wird der Touch kurz gesperrt, und danach
//     muss der Finger einmal ganz weg gewesen sein
// ------------------------------------------------------------
#define TOUCH_PRESS_SAMPLES 2
#define TOUCH_RELEASE_HOLD_MS 60
#define TOUCH_LOCK_MS 350

static void (*s_touch_orig)(lv_indev_drv_t *, lv_indev_data_t *) = NULL;
static uint32_t s_touch_lock_until = 0;
static bool s_touch_wait_release = false;

void ui_input_lock() {
  s_touch_lock_until = lv_tick_get() + TOUCH_LOCK_MS;
  s_touch_wait_release = true;
}

static void touch_filtered_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static bool out = false;       // Zustand, den LVGL sieht
  static uint8_t press_cnt = 0;
  static uint32_t rel_since = 0;
  static lv_point_t last = { 0, 0 }, prev = { 0, 0 }, first = { 0, 0 };
  static uint32_t last_t = 0, prev_t = 0;
  static bool send_first = false;

  s_touch_orig(drv, data);
  bool raw = data->state == LV_INDEV_STATE_PRESSED;
  uint32_t now = lv_tick_get();
  if (raw) {
    prev = last;
    prev_t = last_t;
    last = data->point;
    last_t = now;
  }

#if TOUCH_DEBUG
  static bool raw_prev = false;
  if (raw != raw_prev) printf("Touch %s %d/%d (%lu)\r\n", raw ? "ab" : "auf", last.x, last.y, (unsigned long)now);
  raw_prev = raw;
#endif

  // Sperre nach Seitenwechsel: erst wieder, wenn Zeit um und Finger weg
  if (now < s_touch_lock_until || (s_touch_wait_release && raw)) {
    if (!raw && now >= s_touch_lock_until) s_touch_wait_release = false;
    out = false;
    press_cnt = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    data->point = last;
    return;
  }
  s_touch_wait_release = false;

  lv_point_t p = last;
  if (raw) {
    rel_since = 0;
    if (!out) {
      if (press_cnt == 0) first = last;  // Startpunkt merken
      if (++press_cnt >= TOUCH_PRESS_SAMPLES) {
        out = true;
        send_first = true;  // LVGL zuerst den echten Startpunkt melden, sonst fehlt Wischweg
      }
    }
  } else {
    press_cnt = 0;
    if (out) {
      if (rel_since == 0) rel_since = now;
      if (now - rel_since >= TOUCH_RELEASE_HOLD_MS) {
        out = false;  // wirklich losgelassen
      } else if (last_t > prev_t) {
        // Aussetzer überbrücken: Finger in seiner Richtung weiterlaufen lassen,
        // damit beim Wischen der Schwung erhalten bleibt
        uint32_t dt = now - last_t;
        uint32_t step = last_t - prev_t;
        p.x = last.x + (lv_coord_t)((int32_t)(last.x - prev.x) * (int32_t)dt / (int32_t)step);
        p.y = last.y + (lv_coord_t)((int32_t)(last.y - prev.y) * (int32_t)dt / (int32_t)step);
      }
    }
  }
  if (send_first && out) {
    p = first;
    send_first = false;
  }
  data->state = out ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point = p;
}

// Touch-Verhalten für das runde Display abstimmen und Filter einsetzen
static void tune_touch() {
  for (lv_indev_t *in = lv_indev_get_next(NULL); in; in = lv_indev_get_next(in)) {
    if (in->driver->type != LV_INDEV_TYPE_POINTER) continue;
    if (!s_touch_orig) {
      s_touch_orig = in->driver->read_cb;
      in->driver->read_cb = touch_filtered_read;
    }
    in->driver->scroll_limit = 8;          // ab 8 px Bewegung wird gewischt (Standard 10)
    in->driver->scroll_throw = 8;          // Schwung hält etwas länger (Standard 10)
    in->driver->gesture_limit = 70;        // Zurück-Wischgeste erst ab 70 px (Standard 50)
    in->driver->gesture_min_velocity = 4;  // und mit etwas Tempo
    // Abfragetakt bleibt beim Standard (30 ms): schneller abfragen erzeugt mehr Fehlmeldungen
  }
}

static void home_update_cb(lv_timer_t *t) {
  scale_update();
  net_loop();
  storage_loop();
  voice_loop();
  batt_loop();
  tools_tick();  // Küchentimer, Langzeitmessung
  upd_loop();    // Online-Update: Ton, WLAN freigeben, Neustart

  // Uhrzeit regelmäßig im Flash sichern, falls die RTC beim Ausschalten
  // stromlos ist; beim Start wird sie daraus wiederhergestellt
  {
    static uint32_t last_clock = 0;
    uint32_t now = hal_millis();
    if (now - last_clock > 60000) {
      last_clock = now;
      int y, mo, d, h, mi, sec;
      if (hal_now(&y, &mo, &d, &h, &mi, &sec)) settings_store_time(y, mo, d, h, mi);
    }
  }
  ui_power_tick();

  // BOOT-Taste = Tara (auf jeder Seite)
  bool boot = hal_boot_pressed();
  if (boot && !h_boot_prev) {
    scale_tare();
    sound_play(SND_TARA);
    if (s_pot_mode != POT_NONE) set_pot_mode(POT_NONE, -1);
    if (lv_scr_act() == scr_home) show_toast("Tara gesetzt", C_ACCENT);
  }
  h_boot_prev = boot;

  idle_check();
  if (lv_scr_act() != scr_home) return;

  lv_obj_t *act = lv_tileview_get_tile_act(h_tv);
  if (act == NULL || act == h_tile0) {
    pot_detect();
    autosave_check();
    speak_check();
  }
  if (cd_active) cd_update();

  // Während gewischt wird, die Anzeige nicht neu zeichnen. Jedes neue Gewicht
  // kostet einen Bildaufbau, und dann ruckelt das Wischen oder springt zurück.
  if (touch_busy()) return;

  float gross = scale_gross();
  bool over = scale_overload();
  bool stable = scale_stable();

  // Gewicht
  char buf[48];
  int u = g_set.unit;
  unit_fmt(buf, sizeof(buf), unit_from_g(over ? gross : scale_net(), u), u);
  // Präzisionsmodus (nur in Gramm): Mittelwert über die ruhige Zeit, zwei Nachkommastellen
  float p_net = 0, p_u = 0;
  bool precise = g_set.precise && u == 0 && !over && scale_precise(&p_net, &p_u, NULL);
  if (precise) {
    snprintf(buf, sizeof(buf), "%.2f", (p_net > -0.005f && p_net < 0.005f) ? 0.0f : p_net);
    for (char *c = buf; *c; c++)
      if (*c == '.') *c = ',';
  }
  if (web_hide_weight()) snprintf(buf, sizeof(buf), "? ? ?");  // Schätzspiel im Browser
  weight_show(buf, unit_name(u));
  // Einheit ml: gewählte Flüssigkeit anzeigen (nicht, wenn die Topfzeile dort steht)
  if (u == UNIT_ML && s_pot_mode == POT_NONE) {
    char lb[40], d[12];
    fmt_num(d, sizeof(d), liquid_density(g_set.liquid), 2);
    snprintf(lb, sizeof(lb), "%s · %s g/ml", T(liquid_name(g_set.liquid)), d);
    ui_label_update(h_liquid, lb);
    lv_obj_clear_flag(h_liquid, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(h_liquid, LV_OBJ_FLAG_HIDDEN);
  }

  // Ring = Bruttolast bis Messbereich
  int ring = (int)(gross / WAAGE_MAX_G * 1000.0f);
  if (ring < 0) ring = 0;
  if (ring > 1000) ring = 1000;
  if (lv_arc_get_value(h_ring) != ring) lv_arc_set_value(h_ring, ring);

  // Topfinfo bei abgezogenem Topf
  if (s_pot_mode == POT_LIST) {
    char a[16], p[16];
    unit_fmt(a, sizeof(a), unit_from_g(gross - s_base_tare, u), u);
    unit_fmt(p, sizeof(p), unit_from_g(g_pots[s_pot_idx].grams, u), u);
    snprintf(buf, sizeof(buf), T("brutto %s · Topf %s %s"), a, p, unit_name(u));
    ui_label_update(h_potinfo, buf);
  }

  // Zustand nur bei Änderung umstylen
  int &prev_state = h_prev_state;
  int state = over ? 2 : (precise ? 3 : (stable ? 1 : 0));
  if (state == 3) {  // Unsicherheit ändert sich laufend
    char pb[32], ub[12];
    snprintf(ub, sizeof(ub), "%.2f", p_u);
    for (char *c = ub; *c; c++)
      if (*c == '.') *c = ',';
    snprintf(pb, sizeof(pb), T("präzise ±%s g"), ub);
    ui_chip_set(h_state, pb, C_ACCENT, false);
  }
  if (state != prev_state) {
    prev_state = state;
    lv_color_t col = over ? C_DANGER : C_ACCENT;
    lv_obj_set_style_arc_color(h_ring, col, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(h_weight, over ? C_DANGER : C_TEXT, 0);
    if (state == 2) {
      ui_chip_set(h_state, "Überlast", C_DANGER, false);
      sound_play(SND_OVERLOAD);
      sound_speak_word("ueberlast");  // Stimmpaket (nur mit eingeschalteter Ansage)
    }
    else if (state == 1) ui_chip_set(h_state, g_set.precise && u == 0 ? "mittelt …" : "stabil", C_ACCENT, false);
    else ui_chip_set(h_state, "misst …", C_MUTED, false);
    lv_obj_t *save_lbl = lv_obj_get_child(h_btn_save, 0);
    if (over) lv_obj_add_state(h_btn_save, LV_STATE_DISABLED);
    else lv_obj_clear_state(h_btn_save, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(save_lbl, over ? C_FAINT : C_TEXT, 0);
  }

  // Statuszeile
  int hh, mm;
  int bat = hal_battery_percent();
  char clock[12] = "--:--";
  if (hal_time(&hh, &mm)) snprintf(clock, sizeof(clock), "%02d:%02d", hh, mm);
  const char *sd = storage_ok() ? "SD" : T("keine SD");
  bat_state_t bst = hal_battery_state();
  // Am Ladegerät zeigt die Spannung den Ladestand nicht richtig an,
  // deshalb dort "lädt" bzw. "voll" statt eines falschen Prozentwerts
  if (bst == BAT_CHARGING) snprintf(buf, sizeof(buf), T("%s · %s · lädt"), clock, sd);
  else if (bst == BAT_FULL) snprintf(buf, sizeof(buf), T("%s · %s · voll"), clock, sd);
  else if (bat >= 0) snprintf(buf, sizeof(buf), "%s · %s · %d %%", clock, sd, bat);
  else snprintf(buf, sizeof(buf), "%s · %s", clock, sd);
  // laufender Küchentimer statt der Karte, Langzeitmessung als Hinweis
  int ti = timer_next();
  if (ti >= 0 || lt_running()) {
    char tb[16] = "";
    if (ti >= 0) timer_fmt(tb, sizeof(tb), timer_left(ti));
    if (ti >= 0 && lt_running()) snprintf(buf, sizeof(buf), T("%s · Timer %s · Messung"), clock, tb);
    else if (ti >= 0) snprintf(buf, sizeof(buf), T("%s · Timer %s"), clock, tb);
    else snprintf(buf, sizeof(buf), T("%s · Messung läuft"), clock);
  }
  ui_label_update(h_status, buf);

  // Akkusymbol: Blitz beim Laden, volles Symbol wenn fertig geladen
  if (bst == BAT_CHARGING || bst == BAT_FULL) {
    lv_obj_clear_flag(h_bat, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(h_bat, bst == BAT_CHARGING ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(h_bat, bst == BAT_FULL ? C_ACCENT : C_WARN, 0);
    lv_obj_align_to(h_bat, h_status, LV_ALIGN_OUT_LEFT_MID, -6, 0);
  } else {
    lv_obj_add_flag(h_bat, LV_OBJ_FLAG_HIDDEN);
  }

  // WLAN: grün = Weboberfläche erreichbar, grau = verbindet / holt Uhrzeit
  net_state_t ns = net_state();
  bool wifi = web_running() || ns == NET_CONNECTING || ns == NET_SYNCING;
  if (wifi) {
    lv_obj_clear_flag(h_wifi, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(h_wifi, (web_running() && web_url()[0]) ? C_ACCENT : C_FAINT, 0);
    lv_obj_align_to(h_wifi, h_status, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  } else {
    lv_obj_add_flag(h_wifi, LV_OBJ_FLAG_HIDDEN);
  }
}

// ---------- Aufbau ----------
// ---------- Große Gewichtsanzeige ----------
// 104 px, solange Zahl und Einheit in den runden Bildschirm passen; längere
// Werte (z. B. "-1234,5 g" oder Unzen) schalten auf 80 px zurück.
#define WEIGHT_LETTER_SPACE (-2)
#define WEIGHT_MAX_W 350  // so breit darf die Zeile auf Höhe der Anzeige sein
static const lv_font_t *weight_font = NULL;

static void weight_show(const char *num, const char *unit) {
  lv_point_t n, u;
  lv_txt_get_size(&n, num, &font_sg_104, WEIGHT_LETTER_SPACE, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_txt_get_size(&u, T(unit), &font_sg_34, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const lv_font_t *f = (n.x + 8 + u.x <= WEIGHT_MAX_W) ? &font_sg_104 : &font_sg_80;
  if (f != weight_font) {
    weight_font = f;
    lv_obj_set_style_text_font(h_weight, f, 0);
    // Grundlinie angleichen: Beschriftungen stehen unten bündig, die Grundlinie
    // liegt aber je nach Schriftgröße unterschiedlich hoch über der Unterkante
    lv_obj_set_style_translate_y(h_unit, -(f->base_line - font_sg_34.base_line), 0);
  }
  ui_label_update(h_weight, num);
  ui_label_update(h_unit, unit);
}

static void build_countdown(lv_obj_t *tile) {
  cd_box = ui_box(tile);
  lv_obj_set_size(cd_box, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_set_style_bg_color(cd_box, C_BG, 0);
  lv_obj_set_style_bg_opa(cd_box, LV_OPA_COVER, 0);
  lv_obj_add_flag(cd_box, LV_OBJ_FLAG_CLICKABLE);  // darunterliegende Knöpfe sperren

  cd_ring = ui_ring(cd_box, 396);
  cd_name = ui_label(cd_box, "", &font_sg_24, C_TEXT);
  lv_obj_align(cd_name, LV_ALIGN_CENTER, 0, -86);
  cd_sub = ui_label(cd_box, "", &font_sg_14, C_MUTED);
  lv_obj_align(cd_sub, LV_ALIGN_CENTER, 0, -56);
  cd_secs = ui_label(cd_box, "", &font_sg_80, C_ACCENT);
  lv_obj_align(cd_secs, LV_ALIGN_CENTER, 0, 4);
  cd_hint = ui_label(cd_box, "", &font_sg_18, C_MUTED);
  lv_obj_align(cd_hint, LV_ALIGN_CENTER, 0, 62);
  lv_obj_t *b = ui_btn(cd_box, "Abbrechen", BTN_NORMAL);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 118);
  lv_obj_add_event_cb(b, cd_cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(cd_box, LV_OBJ_FLAG_HIDDEN);
}

static void build_weigh_tile(lv_obj_t *tile) {
  h_ring = ui_ring(tile, 396);

  h_status = ui_label(tile, "--:--", &font_sg_14, C_MUTED);
  lv_obj_align(h_status, LV_ALIGN_TOP_MID, 0, 44);

  // dezentes WLAN-Symbol rechts neben der Statuszeile (Symbolschrift von LVGL)
  h_bat = lv_label_create(tile);
  lv_label_set_text(h_bat, LV_SYMBOL_CHARGE);
  lv_obj_set_style_text_font(h_bat, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h_bat, C_WARN, 0);
  lv_obj_add_flag(h_bat, LV_OBJ_FLAG_HIDDEN);

  h_wifi = lv_label_create(tile);
  lv_label_set_text(h_wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(h_wifi, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h_wifi, C_FAINT, 0);
  lv_obj_add_flag(h_wifi, LV_OBJ_FLAG_HIDDEN);

  h_pot = ui_chip(tile, "", C_ACCENT);
  lv_obj_align(h_pot, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_add_flag(h_pot, LV_OBJ_FLAG_HIDDEN);

  // Gewicht und Einheit in einer Zeile, beide auf derselben Grundlinie (Entwurf A)
  lv_obj_t *wrow = ui_box(tile);
  lv_obj_set_size(wrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(wrow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(wrow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(wrow, 8, 0);
  lv_obj_align(wrow, LV_ALIGN_CENTER, 0, -28);
  h_weight = ui_label(wrow, "0,0", &font_sg_104, C_TEXT);
  lv_obj_set_style_text_letter_space(h_weight, WEIGHT_LETTER_SPACE, 0);
  h_unit = ui_label(wrow, "g", &font_sg_34, C_MUTED);
  // Gewicht lange drücken: Präzisionsmodus an/aus
  lv_obj_add_flag(wrow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(wrow, 20);
  lv_obj_add_event_cb(wrow, [](lv_event_t *e) {
    g_set.precise = !g_set.precise;
    settings_save();
    sound_play(SND_CLICK);
    h_prev_state = -1;
    show_toast(g_set.precise ? "Präzision an" : "Präzision aus", C_ACCENT);
  }, LV_EVENT_LONG_PRESSED, NULL);
  weight_font = NULL;
  weight_show("0,0", "g");

  h_state = ui_chip(tile, "misst …", C_MUTED);
  lv_obj_align(h_state, LV_ALIGN_CENTER, 0, 52);

  h_potinfo = ui_label(tile, "", &font_sg_14, C_MUTED);
  lv_obj_align(h_potinfo, LV_ALIGN_CENTER, 0, 80);

  h_liquid = ui_label(tile, "", &font_sg_14, C_ACCENT);  // oben, wo sonst der Topf steht
  lv_obj_align(h_liquid, LV_ALIGN_TOP_MID, 0, 76);
  lv_obj_add_flag(h_liquid, LV_OBJ_FLAG_HIDDEN);

  // Einheit antippen: bei ml die nächste Flüssigkeit wählen
  lv_obj_add_flag(h_unit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(h_unit, 18);
  lv_obj_add_event_cb(h_unit, [](lv_event_t *e) {
    if (g_set.unit != UNIT_ML) return;
    g_set.liquid = (g_set.liquid + 1) % liquid_count();
    settings_save();
    sound_play(SND_CLICK);
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(h_potinfo, LV_OBJ_FLAG_HIDDEN);

  h_btns = ui_box(tile);
  lv_obj_set_size(h_btns, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(h_btns, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(h_btns, 12, 0);
  lv_obj_align(h_btns, LV_ALIGN_CENTER, 0, 100);
  lv_obj_t *b_tara = ui_btn(h_btns, "Tara", BTN_PRIMARY);
  lv_obj_add_event_cb(b_tara, tara_cb, LV_EVENT_CLICKED, NULL);
  h_btn_save = ui_btn(h_btns, "Speichern", BTN_NORMAL);
  lv_obj_add_event_cb(h_btn_save, save_cb, LV_EVENT_CLICKED, NULL);

  h_btn_remove = ui_btn(tile, "Topf entfernen", BTN_NORMAL);
  lv_obj_align(h_btn_remove, LV_ALIGN_CENTER, 0, 116);
  lv_obj_add_event_cb(h_btn_remove, remove_pot_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(h_btn_remove, LV_OBJ_FLAG_HIDDEN);

  // Lautsprecher-Knopf: während einer Ansage stoppen, sonst den
  // Ansage-Modus durchschalten (Zahlen + Sprüche / Zahlen / Sprüche / aus).
  // Nur sichtbar, wenn ein Stimmpaket auf der SD-Karte liegt.
  h_spk = lv_btn_create(tile);
  lv_obj_remove_style_all(h_spk);
  lv_obj_set_size(h_spk, 38, 38);
  lv_obj_set_style_radius(h_spk, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(h_spk, C_SURFACE, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(h_spk, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(h_spk, 10);
  lv_obj_align(h_spk, LV_ALIGN_CENTER, 0, 162);
  lv_obj_t *spk_ic = ui_label(h_spk, ICON_TON, &font_icons_26, C_MUTED);
  lv_obj_center(spk_ic);
  lv_obj_add_event_cb(h_spk, speaker_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(h_spk, LV_OBJ_FLAG_HIDDEN);

  h_toast = ui_toast_create(tile);
  lv_obj_align(h_toast, LV_ALIGN_CENTER, 0, 152);

  lv_obj_t *dots = ui_page_dots(tile, 3, 0);
  lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -16);

  build_countdown(tile);  // zuletzt, damit es über allem liegt
}

static void build_list_tile(lv_obj_t *tile, const char *title, const ui_list_item_t *items,
                            int count, ui_list_cb_t cb, int page) {
  ui_curved_list(tile, items, count, cb);
  // Titel mit Hintergrund, damit Einträge darunter "wegscrollen"
  lv_obj_t *t = ui_label(tile, title, &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *dots = ui_page_dots(tile, 3, page);
  lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -16);
}

static void build_home_screen() {
  scr_home = ui_screen_create();

  h_tv = lv_tileview_create(scr_home);
  lv_obj_set_size(h_tv, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_set_style_bg_opa(h_tv, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(h_tv, LV_SCROLLBAR_MODE_OFF);

  h_tile0 = lv_tileview_add_tile(h_tv, 0, 0, LV_DIR_RIGHT);
  lv_obj_t *t1 = lv_tileview_add_tile(h_tv, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  lv_obj_t *t2 = lv_tileview_add_tile(h_tv, 2, 0, LV_DIR_LEFT);

  build_weigh_tile(h_tile0);
  build_list_tile(t1, "Modi", MODI_GROUPS, sizeof(MODI_GROUPS) / sizeof(MODI_GROUPS[0]), modi_cb, 1);
  build_list_tile(t2, "System", SYSTEM_ITEMS, sizeof(SYSTEM_ITEMS) / sizeof(SYSTEM_ITEMS[0]), system_cb, 2);
  h_prev_state = -1;
}

static void build_home() {
  build_home_screen();
  lv_timer_create(home_update_cb, 100, NULL);
}

// Sprache umgestellt: Startseite mit den neuen Texten neu aufbauen.
// Unterseiten zeigen die neue Sprache, sobald sie das nächste Mal geöffnet werden.
void ui_lang_changed() {
  lv_obj_t *old = scr_home;
  bool active = lv_scr_act() == old;
  lv_coord_t tile_x = 0;
  if (active) {
    lv_obj_t *act = lv_tileview_get_tile_act(h_tv);
    if (act) tile_x = lv_obj_get_x(act) / SCREEN_SIZE;
  }
  bool cd_was = cd_active;
  cd_active = false;
  build_home_screen();
  // Topfanzeige und Countdown auf die neuen Objekte übertragen
  if (s_pot_mode != POT_NONE && s_pot_idx >= 0 && s_pot_idx < g_pot_count) set_pot_mode(s_pot_mode, s_pot_idx);
  else set_pot_mode(POT_NONE, -1);
  if (cd_was && cd_idx >= 0) {
    uint32_t start = cd_start;
    cd_show(cd_idx, g_pots[cd_idx].grams);
    cd_start = start;
  }
  if (active) {
    lv_obj_set_tile_id(h_tv, tile_x, 0, LV_ANIM_OFF);
    lv_scr_load(scr_home);
  }
  lv_obj_del(old);
  ui_power_lang_changed();
  sound_lang_changed();  // Stimmpaket der neuen Sprache wählen
  settings_save();
}

// ============================================================
//  Lagecheck beim Einschalten
// ============================================================
static lv_obj_t *st_bubble, *st_angle, *st_info, *st_btn;
static lv_timer_t *st_timer;
static uint32_t st_level_since = 0;
static uint32_t st_ok_at = 0;
static bool st_done = false;

static void start_finish() {
  s_start_done = true;
  if (st_timer) {
    lv_timer_del(st_timer);
    st_timer = NULL;
  }
  // Keine Überblendung: halbtransparentes Zeichnen erzeugt helle Kästen
  lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, true);
  // Prüfgewicht fällig? Nach dem Wechsel zur Wiegeseite einmal erinnern
  if (refchk_due()) {
    lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) {
      if (refchk_due()) ui_open_page(page_refdue_create());
    }, 900, NULL);
    lv_timer_set_repeat_count(t, 1);
  }
}

static void start_continue_cb(lv_event_t *e) {
  scale_tare();
  start_finish();
}

static void start_timer_cb(lv_timer_t *t) {
  uint32_t now = hal_millis();
  if (st_done) {
    if (now - st_ok_at >= LEVEL_CONTINUE_MS) start_finish();
    return;
  }

  float tilt, dx, dy;
  if (!ui_read_tilt(&tilt, &dx, &dy)) {
    ui_label_update(st_angle, "keine Lagedaten");
    lv_obj_clear_flag(st_btn, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  char a[12], buf[40];
  ui_fmt_1dec(a, sizeof(a), tilt);

  if (tilt < LEVEL_OK_DEG) {
    if (st_level_since == 0) st_level_since = now;
    ui_bubble_set(st_bubble, dx, dy, C_ACCENT);
    snprintf(buf, sizeof(buf), T("%s° · gerade"), a);
    ui_label_update(st_angle, buf);
    lv_obj_set_style_text_color(st_angle, C_ACCENT, 0);
    lv_obj_add_flag(st_btn, LV_OBJ_FLAG_HIDDEN);

    if (now - st_level_since >= LEVEL_OK_HOLD_MS) {
      // TODO: später prüfen, ob die Waage leer ist (gespeicherter Nullpunkt)
      scale_tare();
      ui_label_update(st_info, "Waage leer · Tara gesetzt");
      lv_obj_clear_flag(st_info, LV_OBJ_FLAG_HIDDEN);
      st_done = true;
      st_ok_at = now;
      sound_play(SND_TARA);
    }
  } else {
    st_level_since = 0;
    ui_bubble_set(st_bubble, dx, dy, C_WARN);
    snprintf(buf, sizeof(buf), T("%s° · schief"), a);
    ui_label_update(st_angle, buf);
    lv_obj_set_style_text_color(st_angle, C_WARN, 0);
    lv_obj_clear_flag(st_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static lv_obj_t *build_start() {
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *title = ui_label(s, "Lagecheck", &font_sg_18, C_MUTED);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 44);

  st_bubble = ui_bubble_create(s, 180);
  lv_obj_align(st_bubble, LV_ALIGN_CENTER, 0, -26);

  st_angle = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(st_angle, LV_ALIGN_CENTER, 0, 88);

  st_info = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(st_info, LV_ALIGN_CENTER, 0, 120);
  lv_obj_add_flag(st_info, LV_OBJ_FLAG_HIDDEN);

  st_btn = ui_btn(s, "Trotzdem weiter", BTN_WARN);
  lv_obj_align(st_btn, LV_ALIGN_CENTER, 0, 138);
  lv_obj_add_flag(st_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(st_btn, start_continue_cb, LV_EVENT_CLICKED, NULL);

  st_timer = lv_timer_create(start_timer_cb, 50, NULL);
  return s;
}

// ============================================================
//  Navigation
// ============================================================
static void page_gesture_cb(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (dir != LV_DIR_RIGHT) return;
  if (s_back) {  // aus einem Modus zurück in seine Gruppe
    page_create_fn fn = s_back;
    s_back = NULL;
    ui_switch_page(fn());
    return;
  }
  ui_go_home();
}

void ui_open_page(lv_obj_t *page) {
  ui_input_lock();  // kein Durchklicken auf die neue Seite
  lv_obj_add_event_cb(page, page_gesture_cb, LV_EVENT_GESTURE, NULL);
  lv_scr_load_anim(page, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}

void ui_switch_page(lv_obj_t *page) {
  ui_input_lock();  // kein Durchklicken auf die neue Seite
  lv_obj_add_event_cb(page, page_gesture_cb, LV_EVENT_GESTURE, NULL);
  // Schieben statt Überblenden (Überblenden lässt Texte kurz weiß aufblitzen)
  lv_scr_load_anim(page, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, true);
}

void ui_go_home() {
  s_back = NULL;
  lv_disp_t *d = lv_disp_get_default();
  if (lv_scr_act() == scr_home || d->scr_to_load == scr_home) return;
  ui_input_lock();
  lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, true);
}

// Webserver öfter bedienen als die Anzeige: Anfragen und DNS-Antworten
// des Captive Portals werden sonst zu langsam beantwortet
static void web_timer_cb(lv_timer_t *t) {
  web_loop();
}

void ui_init() {
  tune_touch();
  lv_timer_create(web_timer_cb, 5, NULL);
  // Kein Standard-Theme: alle Farben kommen aus ui_theme.h
  lv_disp_set_theme(NULL, NULL);

  hal_init();
  settings_load();
  disp_rot_set(g_set.disp_rot);  // Feinausrichtung des Displays (nur wenn != 0)
  storage_begin();  // muss vor sound_begin() laufen, sonst wird die Stimme nicht gefunden
  {  // RTC ohne Strom gewesen? Dann die gesicherte Uhrzeit setzen
    int y, mo, d, h, mi, sec;
    if (!hal_now(&y, &mo, &d, &h, &mi, &sec) && settings_load_time(&y, &mo, &d, &h, &mi))
      hal_set_datetime(y, mo, d, h, mi, 0);
  }
  sound_begin();
  if (g_set.voice_on) voice_begin();  // Spracherkennung, falls eingeschaltet
  pots_load();
  recipes_create_example();
  cocktails_create_examples();
  net_begin();
  scale_init();
  scale_set_autozero(g_set.azt);
  build_home();

  ui_power_init();

  lv_obj_t *start = build_start();
  lv_scr_load_anim(start, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}
