// ============================================================
//  System -> Setup
//   Übersicht: 4 Gruppen (Wiegen, Zeit & WLAN, Ton & Bluetooth, Waage)
//   Auto-Tara:  Topferkennung, Toleranz, Countdown
//   Uhrzeit / Datum: manuell stellen
//   WLAN:       Status, Netz wählen, Passwort über den Ring,
//               Uhrzeit per NTP abgleichen
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_text.h"
#include "settings.h"
#include "hal.h"
#include "net.h"
#include "web.h"
#include "voice.h"
#include "scale.h"
#include "storage.h"
#include "ui_text.h"
#include "sound.h"
#include "data.h"
#include "update_online.h"
#include "refcheck.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *page_autotara_create();
static lv_obj_t *page_time_create();
static lv_obj_t *page_date_create();
static lv_obj_t *page_wlan_create();
static lv_obj_t *page_scan_create();
static lv_obj_t *page_web_create();

lv_obj_t *page_setup_back();

static void back_to_setup_cb(lv_event_t *e) {
  ui_switch_page(page_setup_back());
}

static lv_obj_t *done_btn(lv_obj_t *s, lv_coord_t y) {
  lv_obj_t *b = ui_btn(s, "Fertig", BTN_NORMAL);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, y);
  lv_obj_add_event_cb(b, back_to_setup_cb, LV_EVENT_CLICKED, NULL);
  return b;
}

static lv_obj_t *title(lv_obj_t *s, const char *txt) {
  lv_obj_t *t = ui_label(s, txt, &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);
  return t;
}

// Kippschalter
static void toggle_set(lv_obj_t *sw, bool on) {
  lv_obj_set_style_bg_color(sw, on ? C_ACCENT : C_BORDER, 0);
  lv_obj_align(lv_obj_get_child(sw, 0), on ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, on ? -3 : 3, 0);
}

static lv_obj_t *toggle_create(lv_obj_t *parent, bool on, lv_event_cb_t cb) {
  lv_obj_t *sw = ui_box(parent);
  lv_obj_set_size(sw, 54, 30);
  lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
  lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(sw, 10);
  lv_obj_t *knob = ui_box(sw);
  lv_obj_set_size(knob, 24, 24);
  lv_obj_set_style_radius(knob, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(knob, C_TEXT, 0);
  toggle_set(sw, on);
  lv_obj_add_event_cb(sw, cb, LV_EVENT_CLICKED, NULL);
  return sw;
}

// Zeile: Text links, Inhalt rechts
static lv_obj_t *setting_row(lv_obj_t *s, const char *label, lv_coord_t y) {
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, 270, 52);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, C_TRACK, 0);
  lv_obj_t *l = ui_label(row, label, &font_sg_18, C_TEXT);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  return row;
}

// Spalte mit + / Wert / − (für Uhrzeit und Datum)
static lv_obj_t *stepper_col(lv_obj_t *s, lv_coord_t x, lv_event_cb_t cb, int id) {
  lv_obj_t *plus = ui_round_btn(s, "+", NULL);
  lv_obj_align(plus, LV_ALIGN_CENTER, x, -76);
  lv_obj_add_event_cb(plus, cb, LV_EVENT_CLICKED, (void *)(intptr_t)(id * 2 + 1));
  lv_obj_t *val = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(val, LV_ALIGN_CENTER, x, -10);
  lv_obj_t *minus = ui_round_btn(s, "−", NULL);
  lv_obj_align(minus, LV_ALIGN_CENTER, x, 56);
  lv_obj_add_event_cb(minus, cb, LV_EVENT_CLICKED, (void *)(intptr_t)(id * 2));
  return val;
}

// ------------------------------------------------------------
//  Übersicht
// ------------------------------------------------------------
// ============================================================
//  Übersicht: vier Gruppen als Kacheln (bewusst anders als die
//  Kurvenlisten von Modi/System)
// ============================================================
static int s_cat = 0;  // zuletzt geöffnete Gruppe
static lv_obj_t *page_cat_create(int cat);
static lv_obj_t *page_update_create();
static lv_obj_t *page_voice_pick_create();
static lv_obj_t *page_sound_create();

static const char *const CAT_TITLE[4] = { "Wiegen", "Zeit & Funk", "Ton", "Waage" };
static const char *const CAT_SUB[4] = { "Einheit, Auto-Aus", "Uhr, WLAN, BT", "Töne, Sprache", "Kalibr., Firmware" };
static const char *const CAT_ICON[4] = { ICON_WIEGEN, ICON_ZEIT, ICON_TON, ICON_WAAGE };

lv_obj_t *page_setup_back() {
  return page_cat_create(s_cat);
}

static void tile_cb(lv_event_t *e) {
  s_cat = (int)(intptr_t)lv_event_get_user_data(e);
  ui_switch_page(page_cat_create(s_cat));
}

lv_obj_t *page_setup_create() {
  lv_obj_t *s = ui_screen_create();
  title(s, "Setup");
  static const lv_coord_t X[4] = { -69, 69, -69, 69 };
  static const lv_coord_t Y[4] = { -38, -38, 70, 70 };
  for (int i = 0; i < 4; i++) {
    lv_obj_t *t = ui_box(s);
    lv_obj_set_size(t, 130, 100);
    lv_obj_align(t, LV_ALIGN_CENTER, X[i], Y[i]);
    lv_obj_set_style_radius(t, 26, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(t, C_SURFACE, 0);
    lv_obj_set_style_bg_color(t, lv_color_lighten(C_SURFACE, LV_OPA_20), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_set_style_border_color(t, C_BORDER, 0);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t, tile_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *ic = ui_label(t, CAT_ICON[i], &font_icons_26, C_ACCENT);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t *l = ui_label(t, CAT_TITLE[i], &font_sg_18, C_TEXT);
    lv_obj_set_width(l, 120);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 4);
    lv_obj_t *d = ui_label(t, CAT_SUB[i], &font_sg_14, C_MUTED);
    lv_obj_align(d, LV_ALIGN_BOTTOM_MID, 0, -6);
  }
  return s;
}

// ============================================================
//  Gruppenseiten: Zeilen mit Wert rechts; "›" öffnet eine Unterseite,
//  sonst schaltet Antippen direkt um
// ============================================================
static lv_obj_t *row_val[8];

static void row_value(int cat, int row, char *b, int len) {
  int y, mo, d, h, mi, sec;
  b[0] = 0;
  switch (cat * 10 + row) {
    case 0:
      if (g_set.unit == UNIT_ML) snprintf(b, len, "ml · %.10s", T(liquid_name(g_set.liquid)));
      else snprintf(b, len, "%s", unit_name(g_set.unit));
      break;
    case 1: snprintf(b, len, "%s", g_set.autosave ? T("an") : T("aus")); break;
    case 2: snprintf(b, len, "%s", g_set.auto_next ? T("an") : T("aus")); break;
    case 3:
      if (g_set.idle_min > 0) snprintf(b, len, T("nach %d min"), g_set.idle_min);
      else snprintf(b, len, T("aus"));
      break;
    case 4:
      if (g_set.autotara) snprintf(b, len, T("an · %d s ›"), g_set.countdown_s);
      else snprintf(b, len, T("aus ›"));
      break;
    case 5:
      if (g_set.auto_off_min > 0) snprintf(b, len, T("nach %d min"), g_set.auto_off_min);
      else snprintf(b, len, T("nie"));
      break;
    case 6: snprintf(b, len, "%s", g_set.precise ? T("an") : T("aus")); break;
    case 7: snprintf(b, len, "%s", g_set.azt ? T("an") : T("aus")); break;
    case 10: hal_now(&y, &mo, &d, &h, &mi, &sec); snprintf(b, len, "%02d:%02d ›", h, mi); break;
    case 11:
      if (hal_now(&y, &mo, &d, &h, &mi, &sec)) snprintf(b, len, "%02d.%02d.%04d ›", d, mo, y);
      else snprintf(b, len, T("nicht gestellt ›"));
      break;
    case 12:
      if (!net_has_credentials()) snprintf(b, len, "%s ›", T("einrichten"));
      else if (net_count() > 1) {
        char n[12];
        snprintf(n, sizeof(n), "%.10s", g_set.ssid[0] ? g_set.ssid : net_ssid(0));
        for (int k = strlen(n) - 1; k >= 0 && n[k] == ' '; k--) n[k] = 0;
        snprintf(b, len, "%s +%d ›", n, net_count() - 1);
      }
      else snprintf(b, len, "%.14s ›", net_ssid(0));
      break;
    case 13: snprintf(b, len, "%s ›", web_running() ? T("läuft") : T("starten")); break;
    case 14: snprintf(b, len, T("Kopplung ›")); break;
    case 20:
      if (g_set.volume > 0) snprintf(b, len, "%d %% ›", g_set.volume);
      else snprintf(b, len, T("aus ›"));
      break;
    case 21: snprintf(b, len, "%s", scheme_name(g_set.scheme)); break;
    case 22:
      if (!sound_voice_available()) snprintf(b, len, T("keine Stimme"));
      else snprintf(b, len, "%s", g_set.speak ? T("an") : T("aus"));
      break;
    case 23: {
      static char packs[VOICE_PACKS_MAX][24];
      int n = sound_voice_packs(packs, VOICE_PACKS_MAX);
      if (n == 0) snprintf(b, len, T("keine"));
      else snprintf(b, len, "%.12s ›", g_set.voice[0] ? g_set.voice : T("Ordner"));
      break;
    }
    case 25:
      if (!voice_available()) snprintf(b, len, T("nicht geladen"));
      else if (voice_lack_memory()) snprintf(b, len, T("zu wenig RAM"));
      else if (voice_start_stuck()) snprintf(b, len, T("Start hängt"));
      else if (voice_starting()) snprintf(b, len, T("startet …"));
      else snprintf(b, len, "%s", voice_running() ? T("an") : T("aus"));
      break;
    case 30:
      if (scale_cal_count() > 1) snprintf(b, len, T("%d Punkte ›"), scale_cal_count());
      else snprintf(b, len, T("1 Punkt ›"));
      break;
    case 31: {  // Prüfgewicht
      char w[16];
      if (g_set.ref_g <= 0) { snprintf(b, len, "%s", T("aus ›")); break; }
      int dl = refchk_days_left();
      fmt_num(w, sizeof(w), g_set.ref_g, g_set.ref_g < 100 ? 1 : 0);
      if (dl <= 0) snprintf(b, len, T("fällig ›"));
      else if (dl >= 9999) snprintf(b, len, "%s g ›", w);
      else snprintf(b, len, T("in %d T. ›"), dl);
      break;
    }
    case 32: snprintf(b, len, T("Cg / Cgk ›")); break;
    case 33: snprintf(b, len, T("Nullpunkt ›")); break;
    case 34: snprintf(b, len, T("Klassen ›")); break;
    case 35: snprintf(b, len, "%s", storage_ok() ? T("im Browser ›") : T("keine SD")); break;
    case 24: snprintf(b, len, "%s", g_set.lang == LANG_EN ? "English" : "Deutsch"); break;  // nicht übersetzen
    case 36:
      if (upd_state() == UPD_AVAILABLE) snprintf(b, len, T("%s neu ›"), upd_latest());
      else snprintf(b, len, "%s ›", fw_is_local() ? T("lokal") : fw_version());
      break;
  }
}

static void row_cb(lv_event_t *e) {
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  int row = id % 10;
  static const int IDLE[] = { 0, 1, 2, 5, 10 };
  switch (id) {
    case 0: g_set.unit = (g_set.unit + 1) % UNIT_COUNT; break;
    case 1: g_set.autosave = !g_set.autosave; break;
    case 2: g_set.auto_next = !g_set.auto_next; break;
    case 3: {
      int k = 0;
      while (k < 5 && IDLE[k] != g_set.idle_min) k++;
      g_set.idle_min = IDLE[(k + 1) % 5];
      break;
    }
    case 4: ui_switch_page(page_autotara_create()); return;
    case 5: {
      static const int OFF[] = { 0, 10, 15, 30, 60 };
      int k = 0;
      while (k < 5 && OFF[k] != g_set.auto_off_min) k++;
      g_set.auto_off_min = OFF[(k + 1) % 5];
      break;
    }
    case 6: g_set.precise = !g_set.precise; break;  // Präzisionsmodus
    case 7:  // Nullpunkt-Nachführung
      g_set.azt = !g_set.azt;
      scale_set_autozero(g_set.azt);
      break;
    case 10: ui_switch_page(page_time_create()); return;
    case 11: ui_switch_page(page_date_create()); return;
    case 12: ui_switch_page(page_wlan_create()); return;
    case 13: ui_switch_page(page_web_create()); return;
    case 14: ui_switch_page(page_bluetooth_setup_create()); return;
    case 20: ui_switch_page(page_sound_create()); return;
    case 21:
      g_set.scheme = (g_set.scheme + 1) % SCHEME_COUNT;
      sound_play(SND_TEST);
      break;
    case 22:
      if (!sound_voice_available()) return;
      g_set.speak = !g_set.speak;
      if (g_set.speak) sound_speak_weight(1234.5f, g_set.unit);  // Hörprobe
      break;
    case 23: {  // Stimme: Auswahlliste (bei vielen Paketen besser als Durchtippen)
      static char packs[VOICE_PACKS_MAX][24];
      if (sound_voice_packs(packs, VOICE_PACKS_MAX) == 0) return;
      ui_switch_page(page_voice_pick_create());
      return;
    }
    case 25:
      if (!voice_available() || voice_starting()) return;
      g_set.voice_on = !g_set.voice_on;
      if (g_set.voice_on) voice_begin();  // läuft im Hintergrund an
      else voice_stop();
      break;
    case 30: ui_switch_page(page_kalib_create()); return;
    case 31: ui_switch_page(page_refset_create()); return;
    case 32: ui_switch_page(page_msa_create()); return;
    case 33: ui_switch_page(page_level_setup_create()); return;
    case 34: ui_switch_page(page_porto_setup_create()); return;
    case 35: ui_switch_page(page_web_create()); return;  // Dateien gibt es im Browser
    case 36: ui_switch_page(page_update_create()); return;
    case 24:  // Sprache: Deutsch <-> English
      g_set.lang = g_set.lang == LANG_EN ? LANG_DE : LANG_EN;
      ui_lang_changed();  // Startseite neu aufbauen, Stimmpaket wählen, speichern
      sound_play(SND_CLICK);
      ui_switch_page(page_cat_create(2));  // diese Seite in der neuen Sprache
      return;
  }
  settings_save();  // direkt umgeschaltet
  char b[32];
  if (row >= 8) return;
  row_value(id / 10, row, b, sizeof(b));
  lv_label_set_text(row_val[row], b);
  sound_play(SND_CLICK);
}

static void cat_back_cb(lv_event_t *e) {
  ui_switch_page(page_setup_create());
}

// Solange die Spracherkennung startet, den Wert der Zeile nachführen
static void cat_timer_cb(lv_timer_t *t) {
  int cat = (int)(intptr_t)t->user_data;
  if (cat != 2 || !row_val[5]) return;
  char b[32];
  row_value(2, 5, b, sizeof(b));
  lv_label_set_text(row_val[5], b);
}

static lv_obj_t *page_cat_create(int cat) {
  static const char *const ROWS[4][8] = {
    { "Einheit", "Auto-Speichern", "Auto-Weiter", "Zur Wiegeseite", "Auto-Tara", "Auto-Aus", "Präzision", "Auto-Null" },
    { "Uhrzeit", "Datum", "WLAN", "Weboberfläche", "Bluetooth", NULL, NULL, NULL },
    { "Lautstärke", "Tonschema", "Ansage", "Stimme", "Sprache", "Sprachbefehle", NULL, NULL },
    { "Kalibrierung", "Prüfgewicht", "Messmittelprüfung", "Libelle", "Portoklassen", "Dateien", "Firmware", NULL },
  };
  lv_obj_t *s = ui_screen_create();
  title(s, CAT_TITLE[cat]);

  lv_obj_t *col = ui_box(s);
  lv_obj_set_size(col, 280, 230);  // scrollbar, damit auch 6 Zeilen passen
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(col, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(col, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);
  lv_obj_align(col, LV_ALIGN_CENTER, 0, -4);

  for (int r = 0; r < 8 && ROWS[cat][r]; r++) {
    lv_obj_t *row = ui_box(col);
    lv_obj_set_size(row, 280, 46);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(cat * 10 + r));
    lv_obj_t *l = ui_label(row, ROWS[cat][r], &font_sg_18, C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
    char b[32];
    row_value(cat, r, b, sizeof(b));
    row_val[r] = ui_label(row, b, &font_sg_18, C_ACCENT);
    lv_obj_align(row_val[r], LV_ALIGN_RIGHT_MID, -4, 0);
  }

  if (cat == 2) {
    lv_timer_t *tm = ui_page_timer(s, cat_timer_cb, 500);
    if (tm) tm->user_data = (void *)(intptr_t)cat;
  }

  lv_obj_t *back = ui_btn(s, "Zurück", BTN_NORMAL);
  lv_obj_set_height(back, 46);
  lv_obj_align(back, LV_ALIGN_CENTER, 0, 146);
  lv_obj_add_event_cb(back, cat_back_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

// ------------------------------------------------------------
//  Stimme wählen (alle Pakete der eingestellten Sprache)
// ------------------------------------------------------------
static char vp_names[VOICE_PACKS_MAX][24];

static void vp_pick_cb(int index) {
  sound_voice_set(vp_names[index]);
  settings_save();
  if (g_set.speak) sound_speak_weight(1234.5f, g_set.unit);  // Hörprobe
  else sound_play(SND_CLICK);
  ui_switch_page(page_cat_create(2));
}

static lv_obj_t *page_voice_pick_create() {
  static ui_list_item_t items[VOICE_PACKS_MAX];
  int n = sound_voice_packs(vp_names, VOICE_PACKS_MAX);
  for (int i = 0; i < n; i++) {
    items[i].title = vp_names[i];
    items[i].sub = strcmp(vp_names[i], g_set.voice) == 0 ? T("aktiv") : T("antippen: wählen");
    items[i].icon = NULL;
  }
  lv_obj_t *s = ui_screen_create();
  ui_curved_list(s, items, n, vp_pick_cb);
  lv_obj_t *t = ui_label(s, "Stimme", &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return s;
}

// ------------------------------------------------------------
//  Firmware: Online-Update von GitHub (update_online.*)
// ------------------------------------------------------------
static lv_obj_t *fu_ring, *fu_state, *fu_sub, *fu_btn, *fu_back;
static int fu_prev = -1;

static void fu_timer_cb(lv_timer_t *t) {
  upd_state_t st = upd_state();
  char b[64];
  switch (st) {
    case UPD_IDLE: snprintf(b, sizeof(b), "%s", T("Neue Version auf GitHub suchen")); break;
    case UPD_CONNECT: snprintf(b, sizeof(b), "%s", T("Verbinde mit WLAN …")); break;
    case UPD_CHECK: snprintf(b, sizeof(b), "%s", T("Suche Update …")); break;
    case UPD_LATEST: snprintf(b, sizeof(b), "%s", T("Aktuell, nichts zu tun")); break;
    case UPD_AVAILABLE: snprintf(b, sizeof(b), T("Neu: Version %s"), upd_latest()); break;
    case UPD_DOWNLOAD: snprintf(b, sizeof(b), T("Lade … %d %%"), upd_progress()); break;
    case UPD_DONE: snprintf(b, sizeof(b), "%s", T("Fertig, starte neu …")); break;
    case UPD_ERROR: snprintf(b, sizeof(b), "%s", T(upd_error())); break;
  }
  ui_label_update(fu_state, b);
  int v = st == UPD_DOWNLOAD || st == UPD_DONE ? upd_progress() * 10 : 0;
  if (lv_arc_get_value(fu_ring) != v) lv_arc_set_value(fu_ring, v);
  if ((int)st == fu_prev) return;
  fu_prev = st;
  lv_obj_set_style_text_color(fu_state, st == UPD_ERROR ? C_WARN : (st == UPD_AVAILABLE || st == UPD_DONE ? C_ACCENT : C_TEXT2), 0);
  bool busy = upd_busy() || st == UPD_DONE;
  if (busy) lv_obj_add_flag(fu_btn, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(fu_btn, LV_OBJ_FLAG_HIDDEN);
  if (st == UPD_DOWNLOAD || st == UPD_DONE) lv_obj_add_flag(fu_back, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(fu_back, LV_OBJ_FLAG_HIDDEN);
  bool inst = st == UPD_AVAILABLE;
  lv_label_set_text(lv_obj_get_child(fu_btn, 0), inst ? "Installieren" : "Online suchen");
  lv_obj_set_style_bg_color(fu_btn, inst ? C_ACCENT : C_SURFACE, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(fu_btn, 0), inst ? C_ON_ACCENT : C_TEXT, 0);
  lv_obj_set_style_border_width(fu_btn, inst ? 0 : 2, 0);
  if (st == UPD_DOWNLOAD) ui_label_update(fu_sub, T("Nicht ausschalten"));
  else {
    snprintf(b, sizeof(b), "github.com/%s", upd_repo());
    ui_label_update(fu_sub, b);
  }
}

static lv_obj_t *page_update_create() {
  lv_obj_t *s = ui_screen_create();
  fu_ring = ui_ring(s, 396);
  lv_arc_set_value(fu_ring, 0);
  lv_obj_t *t = ui_label(s, "Firmware", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 58);
  char b[40];
  if (fw_is_local()) snprintf(b, sizeof(b), "%s", T("lokal gebaut"));
  else snprintf(b, sizeof(b), T("Version %s"), fw_version());
  lv_obj_t *v = ui_label(s, b, &font_sg_34, C_TEXT);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, -70);
  fu_state = ui_label(s, "", &font_sg_18, C_TEXT2);
  lv_obj_set_width(fu_state, 300);
  lv_obj_set_style_text_align(fu_state, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(fu_state, LV_LABEL_LONG_WRAP);
  lv_obj_align(fu_state, LV_ALIGN_CENTER, 0, -20);
  fu_sub = ui_label(s, "", &font_sg_14, C_FAINT);
  lv_obj_align(fu_sub, LV_ALIGN_CENTER, 0, 18);
  fu_btn = ui_btn(s, "Online suchen", BTN_NORMAL);
  lv_obj_align(fu_btn, LV_ALIGN_CENTER, 0, 68);
  lv_obj_add_event_cb(fu_btn, [](lv_event_t *e) {
    sound_play(SND_CLICK);
    if (upd_state() == UPD_AVAILABLE) upd_install();
    else upd_check();
  }, LV_EVENT_CLICKED, NULL);
  fu_back = ui_btn(s, "Zurück", BTN_NORMAL);
  lv_obj_set_height(fu_back, 46);
  lv_obj_align(fu_back, LV_ALIGN_CENTER, 0, 132);
  lv_obj_add_event_cb(fu_back, [](lv_event_t *e) { ui_switch_page(page_cat_create(3)); }, LV_EVENT_CLICKED, NULL);
  fu_prev = -1;
  ui_page_timer(s, fu_timer_cb, 200);
  fu_timer_cb(NULL);
  return s;
}

lv_obj_t *page_setup_cat_create(int cat) {
  return page_cat_create(cat);
}

// ------------------------------------------------------------
//  Ton
// ------------------------------------------------------------
static lv_obj_t *snd_val;

static void snd_show() {
  char b[16];
  if (g_set.volume > 0) snprintf(b, sizeof(b), "%d %%", g_set.volume);
  else snprintf(b, sizeof(b), T("aus"));
  lv_label_set_text(snd_val, b);
}

static void snd_adj_cb(lv_event_t *e) {
  int up = (int)(intptr_t)lv_event_get_user_data(e);
  g_set.volume += up ? 10 : -10;
  if (g_set.volume < 0) g_set.volume = 0;
  if (g_set.volume > 100) g_set.volume = 100;
  settings_save();
  snd_show();
  sound_play(SND_TEST);
}

static lv_obj_t *page_sound_create() {
  lv_obj_t *s = ui_screen_create();
  title(s, "Lautstärke");

  snd_val = ui_label(s, "", &font_sg_80, C_TEXT);
  lv_obj_align(snd_val, LV_ALIGN_CENTER, 0, -16);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_align(m, LV_ALIGN_CENTER, -130, -16);
  lv_obj_add_event_cb(m, snd_adj_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_align(p, LV_ALIGN_CENTER, 130, -16);
  lv_obj_add_event_cb(p, snd_adj_cb, LV_EVENT_CLICKED, (void *)1);

  lv_obj_t *h = ui_label(s, "0 % schaltet alle Töne aus", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 70);

  done_btn(s, 124);
  snd_show();
  return s;
}

// ------------------------------------------------------------
//  Auto-Tara
// ------------------------------------------------------------
static lv_obj_t *at_switch, *at_tol, *at_cd;

static void at_refresh() {
  char b[24];
  toggle_set(at_switch, g_set.autotara);
  snprintf(b, sizeof(b), "± %d g / 1 %%", g_set.tol_g);
  lv_label_set_text(lv_obj_get_child(at_tol, 0), b);
  snprintf(b, sizeof(b), "%d s", g_set.countdown_s);
  lv_label_set_text(at_cd, b);
}

static void at_toggle_cb(lv_event_t *e) {
  g_set.autotara = !g_set.autotara;
  settings_save();
  at_refresh();
}

static void at_tol_cb(lv_event_t *e) {
  static const int STEPS[] = { 3, 5, 10, 20 };
  int next = STEPS[0];
  for (int i = 0; i < 4; i++)
    if (STEPS[i] > g_set.tol_g) { next = STEPS[i]; break; }
  g_set.tol_g = next;
  settings_save();
  at_refresh();
}

static void at_cd_cb(lv_event_t *e) {
  int up = (int)(intptr_t)lv_event_get_user_data(e);
  g_set.countdown_s += up ? 1 : -1;
  if (g_set.countdown_s < 1) g_set.countdown_s = 1;
  if (g_set.countdown_s > 10) g_set.countdown_s = 10;
  settings_save();
  at_refresh();
}

static lv_obj_t *page_autotara_create() {
  lv_obj_t *s = ui_screen_create();
  title(s, "Auto-Tara");

  lv_obj_t *r1 = setting_row(s, "Topferkennung", -74);
  at_switch = toggle_create(r1, g_set.autotara, at_toggle_cb);
  lv_obj_align(at_switch, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *r2 = setting_row(s, "Toleranz", -22);
  at_tol = ui_btn(r2, "", BTN_NORMAL);
  lv_obj_set_height(at_tol, 40);
  lv_obj_set_style_pad_hor(at_tol, 12, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(at_tol, 0), &font_sg_14, 0);
  lv_obj_align(at_tol, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(at_tol, at_tol_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *l = ui_label(s, "Countdown", &font_sg_18, C_MUTED);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, 26);
  at_cd = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(at_cd, LV_ALIGN_CENTER, 0, 76);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_align(m, LV_ALIGN_CENTER, -100, 76);
  lv_obj_add_event_cb(m, at_cd_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_align(p, LV_ALIGN_CENTER, 100, 76);
  lv_obj_add_event_cb(p, at_cd_cb, LV_EVENT_CLICKED, (void *)1);

  done_btn(s, 148);
  at_refresh();
  return s;
}

// ------------------------------------------------------------
//  Uhrzeit
// ------------------------------------------------------------
static int t_h, t_m;
static lv_obj_t *t_lh, *t_lm, *t_hint;

static void t_refresh() {
  char b[8];
  snprintf(b, sizeof(b), "%02d", t_h);
  lv_label_set_text(t_lh, b);
  snprintf(b, sizeof(b), "%02d", t_m);
  lv_label_set_text(t_lm, b);
}

static void t_step_cb(lv_event_t *e) {
  int v = (int)(intptr_t)lv_event_get_user_data(e);
  int d = (v & 1) ? 1 : -1;
  if (v / 2 == 0) t_h = (t_h + d + 24) % 24;
  else t_m = (t_m + d + 60) % 60;
  t_refresh();
}

static void t_apply_cb(lv_event_t *e) {
  int y, mo, d, h, mi, s;
  hal_now(&y, &mo, &d, &h, &mi, &s);
  if (y < 2024) { y = 2026; mo = 1; d = 1; }  // Datum noch nicht gestellt
  hal_set_datetime(y, mo, d, t_h, t_m, 0);
  sound_play(SND_SAVE);
  ui_switch_page(page_setup_back());
}

static lv_obj_t *page_time_create() {
  lv_obj_t *s = ui_screen_create();
  int y, mo, d, sec;
  hal_now(&y, &mo, &d, &t_h, &t_m, &sec);
  if (t_h > 23) t_h = 0;
  if (t_m > 59) t_m = 0;
  title(s, "Uhrzeit");

  t_lh = stepper_col(s, -62, t_step_cb, 0);
  lv_obj_t *c = ui_label(s, ":", &font_sg_34, C_TEXT);
  lv_obj_align(c, LV_ALIGN_CENTER, 0, -12);
  t_lm = stepper_col(s, 62, t_step_cb, 1);

  t_hint = ui_label(s, "oder automatisch per WLAN (NTP)", &font_sg_14, C_FAINT);
  lv_obj_align(t_hint, LV_ALIGN_CENTER, 0, 104);

  lv_obj_t *b = ui_btn(s, "Übernehmen", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 146);
  lv_obj_add_event_cb(b, t_apply_cb, LV_EVENT_CLICKED, NULL);

  t_refresh();
  return s;
}

// ------------------------------------------------------------
//  Datum
// ------------------------------------------------------------
static int d_d, d_m, d_y;
static lv_obj_t *d_ld, *d_lm, *d_ly, *d_hint;

static int days_in_month(int m, int y) {
  static const int D[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return D[m - 1];
}

static void d_refresh() {
  char b[8];
  if (d_d > days_in_month(d_m, d_y)) d_d = days_in_month(d_m, d_y);
  snprintf(b, sizeof(b), "%02d", d_d);
  lv_label_set_text(d_ld, b);
  snprintf(b, sizeof(b), "%02d", d_m);
  lv_label_set_text(d_lm, b);
  snprintf(b, sizeof(b), "%04d", d_y);
  lv_label_set_text(d_ly, b);
}

static void d_step_cb(lv_event_t *e) {
  int v = (int)(intptr_t)lv_event_get_user_data(e);
  int k = (v & 1) ? 1 : -1;
  switch (v / 2) {
    case 0: d_d += k; if (d_d < 1) d_d = days_in_month(d_m, d_y); if (d_d > days_in_month(d_m, d_y)) d_d = 1; break;
    case 1: d_m += k; if (d_m < 1) d_m = 12; if (d_m > 12) d_m = 1; break;
    default: d_y += k; if (d_y < 2024) d_y = 2024; if (d_y > 2069) d_y = 2069; break;
  }
  d_refresh();
}

static void d_apply_cb(lv_event_t *e) {
  int y, mo, d, h, mi, s;
  hal_now(&y, &mo, &d, &h, &mi, &s);
  if (h > 23) h = 0;
  if (mi > 59) mi = 0;
  hal_set_datetime(d_y, d_m, d_d, h, mi, s > 59 ? 0 : s);
  sound_play(SND_SAVE);
  ui_switch_page(page_setup_back());
}

static lv_obj_t *page_date_create() {
  lv_obj_t *s = ui_screen_create();
  int h, mi, sec;
  if (!hal_now(&d_y, &d_m, &d_d, &h, &mi, &sec)) {
    d_y = 2026;
    d_m = 1;
    d_d = 1;
  }
  title(s, "Datum");
  d_ld = stepper_col(s, -104, d_step_cb, 0);
  d_lm = stepper_col(s, 0, d_step_cb, 1);
  d_ly = stepper_col(s, 108, d_step_cb, 2);

  lv_obj_t *b = ui_btn(s, "Übernehmen", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 132);
  lv_obj_add_event_cb(b, d_apply_cb, LV_EVENT_CLICKED, NULL);

  d_hint = ui_label(s, "", &font_sg_14, C_ACCENT);
  lv_obj_align(d_hint, LV_ALIGN_CENTER, 0, 96);
  d_refresh();
  return s;
}

// ------------------------------------------------------------
//  WLAN
// ------------------------------------------------------------
static lv_obj_t *w_chip, *w_net, *w_sync, *w_btn_sync, *w_btn_list;
static lv_obj_t *page_wlan_list_create();
static lv_timer_t *w_timer;

static void w_timer_cb(lv_timer_t *t) {
  net_state_t st = net_state();
  bool creds = net_has_credentials();
  switch (st) {
    case NET_CONNECTING: ui_chip_set(w_chip, "verbinde …", C_WARN, false); break;
    case NET_SYNCING: ui_chip_set(w_chip, "hole Uhrzeit …", C_WARN, false); break;
    case NET_OK: ui_chip_set(w_chip, "Uhrzeit abgeglichen", C_ACCENT, false); break;
    case NET_FAIL: ui_chip_set(w_chip, "Verbindung fehlgeschlagen", C_DANGER, false); break;
    default: ui_chip_set(w_chip, creds ? "bereit" : "nicht eingerichtet", C_FAINT, false); break;
  }

  char b[64];
  if (creds) snprintf(b, sizeof(b), T("Netz: %s"), g_set.ssid[0] ? g_set.ssid : net_ssid(0));
  else snprintf(b, sizeof(b), T("Kein Netzwerk gespeichert"));
  ui_label_update(w_net, b);
  snprintf(b, sizeof(b), T("Netzwerke (%d) ›"), net_count());
  lv_label_set_text(lv_obj_get_child(w_btn_list, 0), b);

  uint32_t last = net_last_sync_ms();
  if (last == 0) {
    snprintf(b, sizeof(b), T("Letzter Abgleich: noch nie"));
  } else {
    uint32_t min = (hal_millis() - last) / 60000;
    if (min < 1) snprintf(b, sizeof(b), T("Letzter Abgleich: gerade eben"));
    else if (min < 120) snprintf(b, sizeof(b), T("Letzter Abgleich: vor %lu min"), (unsigned long)min);
    else snprintf(b, sizeof(b), T("Letzter Abgleich: vor %lu h"), (unsigned long)(min / 60));
  }
  ui_label_update(w_sync, b);

  bool busy = st == NET_CONNECTING || st == NET_SYNCING;
  if (!creds || busy) lv_obj_add_state(w_btn_sync, LV_STATE_DISABLED);
  else lv_obj_clear_state(w_btn_sync, LV_STATE_DISABLED);
}

static void w_sync_cb(lv_event_t *e) {
  net_sync_now();
}

static lv_obj_t *page_wlan_create() {
  lv_obj_t *s = ui_screen_create();
  title(s, "WLAN");

  w_chip = ui_chip(s, "", C_FAINT);
  lv_obj_align(w_chip, LV_ALIGN_CENTER, 0, -90);
  w_net = ui_label(s, "", &font_sg_18, C_TEXT);
  lv_obj_align(w_net, LV_ALIGN_CENTER, 0, -50);
  w_sync = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(w_sync, LV_ALIGN_CENTER, 0, -20);

  // gespeicherte Netze (bis zu NET_MAX), dort auch "+ Neues Netzwerk"
  w_btn_list = ui_btn(s, "", BTN_NORMAL);
  lv_obj_align(w_btn_list, LV_ALIGN_CENTER, 0, 30);
  lv_obj_add_event_cb(w_btn_list, [](lv_event_t *e) {
    ui_switch_page(net_count() ? page_wlan_list_create() : page_scan_create());
  }, LV_EVENT_CLICKED, NULL);

  w_btn_sync = ui_btn(s, "Jetzt abgleichen", BTN_PRIMARY);
  lv_obj_align(w_btn_sync, LV_ALIGN_CENTER, 0, 92);
  lv_obj_add_event_cb(w_btn_sync, w_sync_cb, LV_EVENT_CLICKED, NULL);

  done_btn(s, 150);

  w_timer = ui_page_timer(s, w_timer_cb, 500);
  w_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Gespeicherte Netze: Liste, Entfernen
// ------------------------------------------------------------
static int wl_sel = 0;

static lv_obj_t *page_wlan_forget_create() {
  lv_obj_t *s = ui_screen_create();
  title(s, "Netz entfernen?");
  lv_obj_t *n = ui_label(s, net_ssid(wl_sel), &font_sg_24, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t *h = ui_label(s, "Name und Passwort werden gelöscht", &font_sg_14, C_MUTED);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -6);
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 70);
  lv_obj_t *del = ui_btn(row, "Entfernen", BTN_WARN);
  lv_obj_add_event_cb(del, [](lv_event_t *e) {
    net_forget(wl_sel);
    sound_play(SND_CLICK);
    ui_switch_page(net_count() ? page_wlan_list_create() : page_wlan_create());
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *back = ui_btn(row, "Zurück", BTN_NORMAL);
  lv_obj_add_event_cb(back, [](lv_event_t *e) { ui_switch_page(page_wlan_list_create()); }, LV_EVENT_CLICKED, NULL);
  return s;
}

static void wl_pick_cb(int index) {
  if (index == 0) {  // + Neues Netzwerk
    ui_switch_page(page_scan_create());
    return;
  }
  wl_sel = index - 1;
  ui_switch_page(page_wlan_forget_create());
}

static lv_obj_t *page_wlan_list_create() {
  static ui_list_item_t items[NET_MAX + 1];
  static char subs[2][40];
  items[0].title = "+ Neues Netzwerk";
  snprintf(subs[0], sizeof(subs[0]), T("%d von %d gespeichert"), net_count(), NET_MAX);
  items[0].sub = net_count() >= NET_MAX ? T("voll: das älteste fällt heraus") : subs[0];
  items[0].icon = NULL;
  for (int i = 0; i < net_count(); i++) {
    items[i + 1].title = net_ssid(i);
    items[i + 1].sub = strcmp(net_ssid(i), g_set.ssid) == 0 ? T("zuletzt verbunden · antippen: entfernen")
                                                          : T("antippen: entfernen");
    items[i + 1].icon = NULL;
  }
  lv_obj_t *s = ui_screen_create();
  ui_curved_list(s, items, net_count() + 1, wl_pick_cb);
  lv_obj_t *t = ui_label(s, "Gespeicherte Netze", &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return s;
}

// ------------------------------------------------------------
//  Netzwerksuche + Passwort
// ------------------------------------------------------------
#define AP_MAX 16
static net_ap_t s_aps[AP_MAX];
static char s_ssid[33];
static lv_obj_t *sc_screen, *sc_label;
static lv_timer_t *sc_timer;

static void pw_done(bool ok, const char *text) {
  if (ok && text[0]) net_set_credentials(s_ssid, text);
  ui_switch_page(page_wlan_create());
}

static void sc_pick_cb(int index) {
  strncpy(s_ssid, s_aps[index].ssid, sizeof(s_ssid) - 1);
  s_ssid[sizeof(s_ssid) - 1] = 0;
  if (s_aps[index].open) {
    net_set_credentials(s_ssid, "");
    ui_switch_page(page_wlan_create());
    return;
  }
  static char t[48];
  snprintf(t, sizeof(t), T("%s · Passwort"), s_ssid);
  ui_switch_page(ui_text_page_create(t, "", 63, true, NULL, 0, pw_done));
}

static void sc_again_cb(lv_event_t *e) {
  ui_switch_page(page_scan_create());
}

static void sc_timer_cb(lv_timer_t *t) {
  int n = net_scan_get(s_aps, AP_MAX);
  if (n < 0) return;  // läuft noch
  lv_timer_pause(t);  // wird mit der Seite gelöscht
  lv_obj_del(sc_label);

  if (n == 0) {
    lv_obj_t *l = ui_label(sc_screen, "Keine Netze gefunden", &font_sg_18, C_MUTED);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, -20);
    lv_obj_t *b = ui_btn(sc_screen, "Nochmal suchen", BTN_NORMAL);
    lv_obj_align(b, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(b, sc_again_cb, LV_EVENT_CLICKED, NULL);
    return;
  }

  static char subs[AP_MAX][32];
  static ui_list_item_t items[AP_MAX];
  for (int i = 0; i < n; i++) {
    const char *q = s_aps[i].rssi > -60 ? T("stark") : (s_aps[i].rssi > -75 ? T("mittel") : T("schwach"));
    snprintf(subs[i], sizeof(subs[i]), "%s · %s", q, s_aps[i].open ? T("offen") : T("gesichert"));
    items[i].title = s_aps[i].ssid;
    items[i].sub = subs[i];
  }
  ui_curved_list(sc_screen, items, n, sc_pick_cb);
  lv_obj_t *tl = ui_label(sc_screen, "Netzwerke", &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(tl, C_BG, 0);
  lv_obj_set_style_bg_opa(tl, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(tl, 60, 0);
  lv_obj_set_style_pad_top(tl, 26, 0);
  lv_obj_set_style_pad_bottom(tl, 10, 0);
  lv_obj_align(tl, LV_ALIGN_TOP_MID, 0, 0);
}

static lv_obj_t *page_scan_create() {
  sc_screen = ui_screen_create();
  sc_label = ui_label(sc_screen, "Suche Netzwerke …", &font_sg_18, C_MUTED);
  lv_obj_center(sc_label);
  net_scan_start();
  sc_timer = ui_page_timer(sc_screen, sc_timer_cb, 300);
  return sc_screen;
}

// ------------------------------------------------------------
//  Weboberfläche (Design-Sheet: "Setup · Weboberfläche aktiv")
//  Startet WLAN + Server, zeigt Adresse und Restzeit.
// ------------------------------------------------------------
static lv_obj_t *wb_state, *wb_url, *wb_hint, *wb_btn, *wb_qr;

static void wb_timer_cb(lv_timer_t *t) {
  char b[80];
  if (!web_running()) {
    ui_chip_set(wb_state, "aus", C_FAINT, false);
    ui_label_update(wb_url, "");
    ui_label_update(wb_hint, "Starten, um Rezepte, Töpfe und\nEinstellungen am Handy zu bearbeiten");
    lv_label_set_text(lv_obj_get_child(wb_btn, 0), "Starten");
    lv_obj_add_flag(wb_qr, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (web_url()[0]) lv_obj_clear_flag(wb_qr, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(wb_qr, LV_OBJ_FLAG_HIDDEN);
  if (!web_url()[0]) {
    ui_chip_set(wb_state, "verbinde …", C_WARN, false);
    ui_label_update(wb_url, "");
  } else {
    ui_chip_set(wb_state, web_ap_mode() ? "eigenes WLAN" : "im Heimnetz", C_ACCENT, false);
    ui_label_update(wb_url, web_url());
    uint32_t s = web_seconds_left();
    if (web_ap_mode())
      snprintf(b, sizeof(b), T("WLAN: %s\nAus in %lu:%02lu"), web_ssid(), (unsigned long)(s / 60),
               (unsigned long)(s % 60));
    else if (web_update_mode_active())
      snprintf(b, sizeof(b), T("oder http://%s\nUpdate-Modus · noch %lu min wach"), web_ip(),
               (unsigned long)(web_update_seconds_left() / 60 + 1));
    else
      snprintf(b, sizeof(b), T("oder http://%s\nAus in %lu:%02lu ohne Zugriff"), web_ip(), (unsigned long)(s / 60),
               (unsigned long)(s % 60));
    ui_label_update(wb_hint, b);
  }
  lv_label_set_text(lv_obj_get_child(wb_btn, 0), "Beenden");
}

static void wb_btn_cb(lv_event_t *e) {
  if (web_running()) web_stop();
  else web_start();
  wb_timer_cb(NULL);
}

static lv_obj_t *page_web_create() {
  lv_obj_t *s = ui_screen_create();
  title(s, "Weboberfläche");

  wb_state = ui_chip(s, "", C_FAINT);
  lv_obj_align(wb_state, LV_ALIGN_CENTER, 0, -100);

  wb_url = ui_label(s, "", &font_sg_24, C_TEXT);
  lv_obj_align(wb_url, LV_ALIGN_CENTER, 0, -62);

  wb_hint = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(wb_hint, LV_ALIGN_CENTER, 0, -16);

  // QR-Code zum Scannen (nur solange die Weboberfläche läuft)
  wb_qr = ui_btn(s, "QR-Code ›", BTN_NORMAL);
  lv_obj_set_height(wb_qr, 40);
  lv_obj_set_style_text_font(lv_obj_get_child(wb_qr, 0), &font_sg_14, 0);
  lv_obj_align(wb_qr, LV_ALIGN_CENTER, 0, 32);
  lv_obj_add_event_cb(wb_qr, [](lv_event_t *e) { ui_switch_page(page_wlan_qr_create()); }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 86);
  lv_obj_t *upd = ui_btn(row, "Update-Modus", BTN_NORMAL);
  lv_obj_add_event_cb(upd, [](lv_event_t *e) {
    web_update_mode(15);  // 15 Minuten wach halten
    sound_play(SND_TICK);
  }, LV_EVENT_CLICKED, NULL);
  wb_btn = ui_btn(row, "Starten", BTN_PRIMARY);
  lv_obj_add_event_cb(wb_btn, wb_btn_cb, LV_EVENT_CLICKED, NULL);

  done_btn(s, 144);
  ui_page_timer(s, wb_timer_cb, 500);
  wb_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  QR-Codes: WLAN der Waage (Handy verbindet sich per Kamera)
//  und Adresse der Weboberfläche. Antippen wechselt.
//  Braucht LV_USE_QRCODE 1 in lv_conf.h (siehe README).
// ------------------------------------------------------------
static int qr_mode = 0;  // 0 = WLAN, 1 = Adresse
static lv_obj_t *qr_code, *qr_caption, *qr_hint;

// Sonderzeichen im WLAN-Namen für den QR-Text maskieren (\ ; , : ")
static void qr_escape(char *out, int len, const char *in) {
  int k = 0;
  for (; *in && k < len - 2; in++) {
    if (strchr("\;,:\"", *in)) out[k++] = '\\';
    out[k++] = *in;
  }
  out[k] = 0;
}

static void qr_show() {
  char data[128], cap[64];
  if (!web_ap_mode()) qr_mode = 1;  // im Heimnetz ist das Handy schon im richtigen WLAN
  if (qr_mode == 0) {
    char ssid[64];
    qr_escape(ssid, sizeof(ssid), web_ssid());
    snprintf(data, sizeof(data), "WIFI:T:nopass;S:%s;;", ssid);  // offenes WLAN der Waage
    snprintf(cap, sizeof(cap), T("WLAN „%s“"), web_ssid());
  } else {
    snprintf(data, sizeof(data), "%s", web_url());
    snprintf(cap, sizeof(cap), "%s", web_url());
  }
#if LV_USE_QRCODE
  lv_qrcode_update(qr_code, data, strlen(data));
#endif
  lv_label_set_text(qr_caption, cap);
  if (!web_ap_mode()) lv_label_set_text(qr_hint, "Mit der Kamera scannen");
  else lv_label_set_text(qr_hint, qr_mode == 0 ? "Antippen: Adresse" : "Antippen: WLAN");
}

static void qr_tap_cb(lv_event_t *e) {
  if (!web_ap_mode()) return;
  qr_mode = 1 - qr_mode;
  sound_play(SND_CLICK);
  qr_show();
}

static void qr_timer_cb(lv_timer_t *t) {
  if (!web_running() || !web_url()[0]) ui_switch_page(page_web_create());  // abgelaufen
}

lv_obj_t *page_wlan_qr_create() {
  lv_obj_t *s = ui_screen_create();
  title(s, "QR-Code");

  // weißer Rand (Ruhezone), sonst erkennen manche Kameras den Code schlecht
  lv_obj_t *box = ui_box(s);
  lv_obj_set_size(box, 188, 188);
  lv_obj_set_style_bg_color(box, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 14, 0);
  lv_obj_align(box, LV_ALIGN_CENTER, 0, -14);
  lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(box, qr_tap_cb, LV_EVENT_CLICKED, NULL);
#if LV_USE_QRCODE
  qr_code = lv_qrcode_create(box, 164, lv_color_black(), lv_color_white());
  lv_obj_center(qr_code);
  lv_obj_clear_flag(qr_code, LV_OBJ_FLAG_CLICKABLE);
#else
  qr_code = ui_label(box, "LV_USE_QRCODE\nin lv_conf.h\nauf 1 setzen", &font_sg_14, lv_color_black());
  lv_obj_set_style_text_align(qr_code, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(qr_code);
#endif

  qr_caption = ui_label(s, "", &font_sg_18, C_TEXT);
  lv_obj_align(qr_caption, LV_ALIGN_CENTER, 0, 96);
  qr_hint = ui_label(s, "", &font_sg_14, C_FAINT);
  lv_obj_align(qr_hint, LV_ALIGN_CENTER, 0, 120);

  lv_obj_t *b = ui_btn(s, "Fertig", BTN_NORMAL);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 164);
  lv_obj_add_event_cb(b, [](lv_event_t *e) { ui_switch_page(page_web_create()); }, LV_EVENT_CLICKED, NULL);

  qr_mode = 0;
  qr_show();
  ui_page_timer(s, qr_timer_cb, 1000);
  return s;
}
