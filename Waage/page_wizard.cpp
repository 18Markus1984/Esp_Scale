// ============================================================
//  Einrichtung beim ersten Start (Assistent)
//
//  Schritte: Sprache, Profil, Kategorien (nicht bei Einfach),
//  Farbe, Ton, WLAN. Jeder Schritt hat „Zurück“ und „Überspringen“
//  (dann bleibt der bisherige Wert). Danach folgt beim ersten Start
//  der Lagecheck, beim Wiederholen (Setup -> Waage -> Einrichtung)
//  die Wiegeseite. Derselbe Assistent steht in der Weboberfläche.
//
//  Während der Assistent läuft, führen „Fertig“-Knöpfe und Wischen
//  auf Unterseiten (WLAN, Weboberfläche) zurück zum aktuellen Schritt
//  statt zur Wiegeseite (ui_set_home_override).
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "settings.h"
#include "sound.h"
#include "net.h"
#include "i18n.h"
#include <stdio.h>
#include <string.h>

enum { WZ_LANG, WZ_PROFILE, WZ_CATS, WZ_COLOR, WZ_SOUND, WZ_WLAN, WZ_COUNT };

static int s_step = 0;
static bool s_active = false;
static void (*s_done)() = NULL;  // erster Start: danach Lagecheck; sonst NULL = Wiegeseite

static lv_obj_t *wz_page(int step);
static lv_obj_t *wz_current() { return wz_page(s_step); }

bool wizard_active() { return s_active; }

// Kategorien entfallen im Profil Einfach
static bool step_used(int st) {
  return !(st == WZ_CATS && g_set.profile == PROFILE_EINFACH);
}

static int step_number(int st, int *total) {
  int n = 0, t = 0;
  for (int i = 0; i < WZ_COUNT; i++) {
    if (!step_used(i)) continue;
    t++;
    if (i <= st) n = t;
  }
  if (total) *total = t;
  return n;
}

static void wz_finish() {
  s_active = false;
  ui_set_back(NULL);
  ui_set_home_override(NULL);
  page_setup_back_override(NULL);
  g_set.setup_done = true;
  ui_profile_changed();  // Farben, Startseite, speichern
  sound_play(SND_SAVE);
  void (*d)() = s_done;
  s_done = NULL;
  if (d) d();
  else ui_go_home();
}

// Die Weboberfläche hat die Einrichtung abgeschlossen: Assistent am Gerät beenden
void wizard_finish_external() {
  if (s_active) wz_finish();
}

static void wz_go(int step) {
  s_step = step;
  ui_switch_page(wz_page(step));
}

static void wz_next() {
  int st = s_step + 1;
  while (st < WZ_COUNT && !step_used(st)) st++;
  if (st >= WZ_COUNT) {
    wz_finish();
    return;
  }
  wz_go(st);
}

static lv_obj_t *wz_prev_page() {
  int st = s_step - 1;
  while (st > 0 && !step_used(st)) st--;
  if (st < 0) st = 0;
  s_step = st;
  return wz_page(st);
}

static void next_cb(lv_event_t *e) { wz_next(); }
static void back_cb(lv_event_t *e) { ui_switch_page(wz_prev_page()); }

// ---------- gemeinsamer Rahmen: Kopfzeile und Zurück/Überspringen ----------
static void frame(lv_obj_t *s, const char *title_txt, bool list) {
  int total, n = step_number(s_step, &total);
  char b[48];
  // Listen haben keinen Platz für einen großen Titel: dann steht er in der Kopfzeile
  if (list && title_txt) snprintf(b, sizeof(b), "%s · %d / %d", T(title_txt), n, total);
  else snprintf(b, sizeof(b), T("Einrichtung · %d / %d"), n, total);
  lv_obj_t *h = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(h, C_BG, 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(h, 60, 0);
  lv_obj_set_style_pad_top(h, 26, 0);
  lv_obj_set_style_pad_bottom(h, list ? 10 : 4, 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 0);
  if (title_txt && !list) {
    lv_obj_t *t = ui_label(s, title_txt, &font_sg_24, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);
  }

  // unten: Zurück (nicht im ersten Schritt) und Überspringen, auf eigenem Hintergrund
  lv_obj_t *bar = ui_box(s);
  lv_obj_set_size(bar, SCREEN_SIZE, 76);
  lv_obj_set_style_bg_color(bar, C_BG, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
  if (s_step > 0) {
    lv_obj_t *bk = ui_label(bar, "‹ Zurück", &font_sg_18, C_MUTED);
    lv_obj_add_flag(bk, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(bk, 14);
    lv_obj_align(bk, LV_ALIGN_TOP_MID, -76, 14);
    lv_obj_add_event_cb(bk, back_cb, LV_EVENT_CLICKED, NULL);
  }
  lv_obj_t *sk = ui_label(bar, "Überspringen ›", &font_sg_18, C_MUTED);
  lv_obj_add_flag(sk, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(sk, 14);
  lv_obj_align(sk, LV_ALIGN_TOP_MID, s_step > 0 ? 70 : 0, 14);
  lv_obj_add_event_cb(sk, next_cb, LV_EVENT_CLICKED, NULL);

  // Wischen nach rechts = Zurück (im ersten Schritt bleibt man stehen)
  if (s_step > 0) ui_set_back(wz_prev_page);
}

static lv_obj_t *big_btn(lv_obj_t *s, const char *txt, bool active, lv_coord_t y) {
  lv_obj_t *b = ui_btn(s, txt, active ? BTN_PRIMARY : BTN_NORMAL);
  lv_obj_set_size(b, 250, 62);
  lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &font_sg_24, 0);
  lv_obj_center(lv_obj_get_child(b, 0));
  lv_obj_align(b, LV_ALIGN_CENTER, 0, y);
  return b;
}

// ---------- 1. Sprache ----------
static void lang_cb(lv_event_t *e) {
  int l = (int)(intptr_t)lv_event_get_user_data(e);
  if (l != g_set.lang) {
    g_set.lang = l;
    ui_lang_changed();  // Startseite, Stimmpaket, speichern
  }
  sound_play(SND_CLICK);
  wz_next();
}

static lv_obj_t *page_lang() {
  lv_obj_t *s = ui_screen_create();
  frame(s, NULL, false);
  // Titel zweisprachig, damit man ihn auch ohne die andere Sprache versteht (nicht übersetzen)
  lv_obj_t *t = lv_label_create(s);
  lv_label_set_text_static(t, "Sprache · Language");
  lv_obj_set_style_text_font(t, &font_sg_24, 0);
  lv_obj_set_style_text_color(t, C_TEXT, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_t *de = big_btn(s, "Deutsch", g_set.lang == LANG_DE, -30);
  lv_obj_add_event_cb(de, lang_cb, LV_EVENT_CLICKED, (void *)(intptr_t)LANG_DE);
  lv_obj_t *en = big_btn(s, "English", g_set.lang == LANG_EN, 44);
  lv_obj_add_event_cb(en, lang_cb, LV_EVENT_CLICKED, (void *)(intptr_t)LANG_EN);
  // Beschriftungen nicht übersetzen
  lv_label_set_text_static(lv_obj_get_child(de, 0), "Deutsch");
  lv_label_set_text_static(lv_obj_get_child(en, 0), "English");

  // Alternativ alles am Handy einrichten (startet die Weboberfläche mit QR-Code)
  lv_obj_t *hd = ui_label(s, "Mit dem Handy einrichten ›", &font_sg_14, C_ACCENT);
  lv_obj_add_flag(hd, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(hd, 14);
  lv_obj_align(hd, LV_ALIGN_CENTER, 0, 104);
  lv_obj_add_event_cb(hd, [](lv_event_t *e) {
    sound_play(SND_CLICK);
    ui_switch_page(page_web_quick_create());
  }, LV_EVENT_CLICKED, NULL);
  return s;
}

// ---------- 2. Profil ----------
static const char *const PROF_NAMES[PROFILE_COUNT] = { "Standard", "Küche", "Werkstatt", "Einfach" };
static const char *const PROF_SUB[PROFILE_COUNT] = { "alle Modi", "Rezept, Timer, Töpfe", "Zählen, Spule, Porto",
                                                     "nur Wiegen, groß, mit Ansage" };

static void profile_cb(int index) {
  if (index < 0 || index >= PROFILE_COUNT) return;
  settings_profile_apply(index);  // Startseite wird am Ende aufgebaut
  sound_play(SND_CLICK);
  wz_next();
}

static lv_obj_t *page_profile() {
  static ui_list_item_t items[PROFILE_COUNT];
  static const char *const ICONS[PROFILE_COUNT] = { ICON_WAAGE, ICON_KUECHE, ICON_WERKSTATT, ICON_WIEGEN };
  for (int i = 0; i < PROFILE_COUNT; i++) {
    items[i].title = T(PROF_NAMES[i]);
    items[i].sub = T(PROF_SUB[i]);
    items[i].icon = ICONS[i];
  }
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *list = ui_curved_list(s, items, PROFILE_COUNT, profile_cb);
  lv_obj_scroll_to_view(lv_obj_get_child(list, g_set.profile), LV_ANIM_OFF);
  frame(s, "Profil", true);
  return s;
}

// ---------- 3. Kategorien ----------
static lv_obj_t *cat_val[3];

static void cat_show() {
  for (int i = 0; i < 3; i++)
    if (cat_val[i]) lv_label_set_text(cat_val[i], (g_set.cats & (1 << i)) ? "an" : "aus");
}

static void cat_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  g_set.cats ^= (uint8_t)(1 << i);
  sound_play(SND_CLICK);
  cat_show();
}

static lv_obj_t *page_cats() {
  static const char *const NAMES[3] = { "Küche", "Werkstatt", "Spiele" };
  lv_obj_t *s = ui_screen_create();
  frame(s, "Kategorien", false);
  lv_obj_t *h = ui_label(s, "Welche Modi soll die Waage zeigen?", &font_sg_14, C_MUTED);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 92);
  for (int i = 0; i < 3; i++) {
    lv_obj_t *row = ui_box(s);
    lv_obj_set_size(row, 270, 46);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -54 + i * 48);
    lv_obj_add_event_cb(row, cat_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *l = ui_label(row, NAMES[i], &font_sg_18, C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
    cat_val[i] = ui_label(row, "", &font_sg_18, C_VALUE);
    lv_obj_align(cat_val[i], LV_ALIGN_RIGHT_MID, -4, 0);
  }
  cat_show();
  lv_obj_t *b = ui_btn(s, "Weiter", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 104);
  lv_obj_add_event_cb(b, next_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { memset(cat_val, 0, sizeof(cat_val)); }, LV_EVENT_DELETE, NULL);
  return s;
}

// ---------- 4. Farbe ----------
static void color_cb(int index) {
  if (index < 0 || index >= THEME_COUNT) return;
  g_set.theme = index;
  ui_theme_set(index);  // die nächsten Schritte schon in der neuen Farbe
  sound_play(SND_CLICK);
  wz_next();
}

static lv_obj_t *page_color() {
  static ui_list_item_t items[THEME_COUNT];
  for (int i = 0; i < THEME_COUNT; i++) {
    items[i].title = T(ui_theme_name(i));
    items[i].sub = i == g_set.theme ? T("aktuell") : T("antippen: wählen");
    items[i].icon = NULL;
  }
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *list = ui_curved_list(s, items, THEME_COUNT, color_cb);
  for (int i = 0; i < THEME_COUNT; i++) {  // Farbpunkt links in jedem Eintrag
    lv_obj_t *dot = ui_box(lv_obj_get_child(list, i));
    lv_obj_set_size(dot, 22, 22);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, ui_theme_color(i), 0);
    lv_obj_set_style_border_width(dot, 2, 0);
    lv_obj_set_style_border_color(dot, C_BG, 0);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 26, 0);
  }
  int cur = g_set.theme >= 0 && g_set.theme < THEME_COUNT ? g_set.theme : 0;
  lv_obj_scroll_to_view(lv_obj_get_child(list, cur), LV_ANIM_OFF);
  frame(s, "Farbe", true);
  return s;
}

// ---------- 5. Ton ----------
static lv_obj_t *snd_vol, *snd_speak;

static void snd_show() {
  char b[16];
  if (snd_vol) {
    if (g_set.volume > 0) snprintf(b, sizeof(b), "%d %%", g_set.volume);
    else snprintf(b, sizeof(b), "%s", T("aus"));
    lv_label_set_text(snd_vol, b);
  }
  if (snd_speak) lv_label_set_text(snd_speak, g_set.speak ? "an" : "aus");
}

static void vol_cb(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  int v = g_set.volume + d;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  g_set.volume = v;
  snd_show();
  sound_play(SND_TEST);  // Hörprobe in der neuen Lautstärke
}

static void speak_cb(lv_event_t *e) {
  g_set.speak = !g_set.speak;
  g_set.speak_ev = g_set.speak;
  snd_show();
  if (g_set.speak) sound_speak_weight_now(250.0f, 0);  // Hörprobe der Ansage
  else sound_play(SND_CLICK);
}

static lv_obj_t *round_btn(lv_obj_t *parent, const char *txt, lv_event_cb_t cb, int d) {
  lv_obj_t *b = ui_btn(parent, txt, BTN_NORMAL);
  lv_obj_set_size(b, 56, 56);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &font_sg_34, 0);
  lv_obj_center(lv_obj_get_child(b, 0));
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)d);
  return b;
}

static lv_obj_t *page_sound() {
  lv_obj_t *s = ui_screen_create();
  frame(s, "Ton", false);
  lv_obj_t *l = ui_label(s, "Lautstärke", &font_sg_14, C_MUTED);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, -86);
  lv_obj_t *minus = round_btn(s, "-", vol_cb, -10);
  lv_obj_align(minus, LV_ALIGN_CENTER, -104, -44);
  lv_obj_t *plus = round_btn(s, "+", vol_cb, 10);
  lv_obj_align(plus, LV_ALIGN_CENTER, 104, -44);
  snd_vol = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(snd_vol, LV_ALIGN_CENTER, 0, -44);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, 260, 46);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, C_TRACK, 0);
  lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 30);
  lv_obj_add_event_cb(row, speak_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *rl = ui_label(row, "Gewicht ansagen", &font_sg_18, C_TEXT);
  lv_obj_align(rl, LV_ALIGN_LEFT_MID, 4, 0);
  snd_speak = ui_label(row, "", &font_sg_18, C_VALUE);
  lv_obj_align(snd_speak, LV_ALIGN_RIGHT_MID, -4, 0);
  snd_show();

  lv_obj_t *b = ui_btn(s, "Weiter", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 104);
  lv_obj_add_event_cb(b, next_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    snd_vol = NULL;
    snd_speak = NULL;
  }, LV_EVENT_DELETE, NULL);
  return s;
}

// ---------- 6. WLAN ----------
static lv_obj_t *wl_state;

static void wl_timer_cb(lv_timer_t *t) {
  char b[64];
  if (!wl_state) return;
  net_state_t st = net_state();
  if (!net_has_credentials()) snprintf(b, sizeof(b), "%s", T("noch nicht eingerichtet"));
  else if (st == NET_CONNECTING || st == NET_SYNCING) snprintf(b, sizeof(b), T("verbinde mit %.24s …"), net_ssid(0));
  else if (st == NET_FAIL) snprintf(b, sizeof(b), T("%.24s · keine Verbindung"), net_ssid(0));
  else snprintf(b, sizeof(b), T("Netz: %.24s"), net_ssid(0));
  ui_label_update(wl_state, b);
}

static lv_obj_t *page_wlan() {
  lv_obj_t *s = ui_screen_create();
  frame(s, "WLAN", false);
  lv_obj_t *h = ui_label(s, "Für Uhrzeit, Weboberfläche und Updates. Geht auch später im Setup.", &font_sg_18,
                         C_MUTED);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, 290);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -58);
  wl_state = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_align(wl_state, LV_ALIGN_CENTER, 0, -4);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 64);
  lv_obj_t *set = ui_btn(row, "Einrichten", BTN_NORMAL);
  lv_obj_add_event_cb(set, [](lv_event_t *e) {
    page_setup_back_override(wz_current);  // „Fertig“ auf der WLAN-Seite kommt hierher zurück
    ui_switch_page(page_wlan_setup_create());
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *done = ui_btn(row, "Fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(done, next_cb, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, wl_timer_cb, 500);
  wl_timer_cb(NULL);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { wl_state = NULL; }, LV_EVENT_DELETE, NULL);
  return s;
}

static lv_obj_t *wz_page(int step) {
  switch (step) {
    case WZ_PROFILE: return page_profile();
    case WZ_CATS: return page_cats();
    case WZ_COLOR: return page_color();
    case WZ_SOUND: return page_sound();
    case WZ_WLAN: return page_wlan();
    default: return page_lang();
  }
}

// ---------- Einstieg ----------
static void wz_begin(void (*done)()) {
  s_active = true;
  s_done = done;
  s_step = WZ_LANG;
  ui_set_home_override(wz_current);  // Unterseiten führen zum aktuellen Schritt zurück
}

void wizard_start(void (*done)()) {
  wz_begin(done);
  ui_switch_page(wz_page(WZ_LANG));
}

lv_obj_t *page_wizard_create() {
  wz_begin(NULL);
  return wz_page(WZ_LANG);
}
