// ============================================================
//  Einrichtung beim ersten Start (Assistent)
//
//  Schritte: Sprache, Profil, Kategorien (nicht bei Einfach),
//  Farbe, Ton, WLAN, danach „Probier es aus“: die Bedienung wird
//  zum Mitmachen erklärt (links/rechts wischen, Zahl halten; im
//  Profil Einfach oben und unten tippen). Unten links und rechts
//  sitzen zwei Pfeil-Knöpfe (zurück / weiter), der Fortschritt läuft
//  als Bogen um den Rand. Danach folgt beim ersten Start der Lagecheck,
//  beim Wiederholen (Setup -> Waage -> Einrichtung) die Wiegeseite.
//  Derselbe Assistent (ohne Erklärung) steht in der Weboberfläche.
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

enum { WZ_LANG, WZ_PROFILE, WZ_CATS, WZ_COLOR, WZ_SOUND, WZ_WLAN,
       WZ_TUT1, WZ_TUT2, WZ_TUT3,  // Bedienung ausprobieren (Einfach: nur 1 und 2)
       WZ_COUNT };
#define WZ_SETUP_STEPS WZ_TUT1  // Einrichtungsschritte vor der Erklärung

static int s_step = 0;
static bool s_active = false;
static void (*s_done)() = NULL;  // erster Start: danach Lagecheck; sonst NULL = Wiegeseite

static lv_obj_t *wz_page(int step);
static lv_obj_t *wz_current() { return wz_page(s_step); }

// Beim Löschen einer Seite einen Zeiger nur vergessen, wenn er zu dieser Seite gehört.
// (Die alte Seite wird erst gelöscht, wenn die neue schon steht; deren Zeiger bleiben.)
static void forget(lv_obj_t **p, lv_event_t *e) {
  if (*p && lv_obj_get_screen(*p) == lv_event_get_target(e)) *p = NULL;
}

bool wizard_active() { return s_active; }

static bool is_tut(int st) { return st >= WZ_TUT1; }

// Kategorien und der dritte Übungsschritt entfallen im Profil Einfach
static bool step_used(int st) {
  if (g_set.profile == PROFILE_EINFACH && (st == WZ_CATS || st == WZ_TUT3)) return false;
  return true;
}

// Nummer und Anzahl innerhalb der Gruppe (Einrichtung bzw. Ausprobieren)
static int step_number(int st, int *total) {
  int from = is_tut(st) ? WZ_TUT1 : 0, to = is_tut(st) ? WZ_COUNT : WZ_SETUP_STEPS;
  int n = 0, t = 0;
  for (int i = from; i < to; i++) {
    if (!step_used(i)) continue;
    t++;
    if (i <= st) n = t;
  }
  if (total) *total = t;
  return n;
}

// Fortschritt über alles (Bogen am Rand), 0..1000
static int progress(int st) {
  int n = 0, t = 0;
  for (int i = 0; i < WZ_COUNT; i++) {
    if (!step_used(i)) continue;
    t++;
    if (i <= st) n = t;
  }
  return t ? n * 1000 / t : 0;
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

// Übungsseiten werden ohne die allgemeine Wisch-Geste geladen: dort ist
// Wischen die Aufgabe und wird von der Seite selbst ausgewertet
static void wz_load(lv_obj_t *page, bool forward) {
  if (!is_tut(s_step)) {
    ui_switch_page(page);
    return;
  }
  ui_input_lock();
  // Kam der Wechsel aus einer Wischgeste, den Zustand des Touch zurücksetzen,
  // sonst erkennt LVGL beim nächsten Drücken kein langes Drücken
  lv_indev_t *in = lv_indev_get_act();
  if (in) lv_indev_reset(in, NULL);
  lv_scr_load_anim(page, forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, true);
}

static void wz_go(int step) {
  s_step = step;
  wz_load(wz_page(step), true);
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
static void back_cb(lv_event_t *e) {
  lv_obj_t *p = wz_prev_page();
  wz_load(p, false);
}

// ---------- gemeinsamer Rahmen ----------
// Kopfzeile oben; am Ende jeder Seite nav(): Fortschrittsbogen am Rand und
// die beiden Pfeil-Knöpfe unten links und rechts (innerhalb des Bogens,
// damit sich Bogen und Knöpfe nicht berühren)
#define NAV_R   150  // Abstand der Knopfmitte vom Bildschirmmittelpunkt (Bogen innen: 188)
#define NAV_BTN 54
#define LIST_SHIFT (-30)  // gebogene Listen etwas nach oben, damit der Eintrag unter der Mitte sichtbar bleibt

static void frame(lv_obj_t *s, const char *title_txt, bool list) {
  int total, n = step_number(s_step, &total);
  char b[48];
  if (is_tut(s_step)) snprintf(b, sizeof(b), T("Probier es aus · %d / %d"), n, total);
  else if (list && title_txt) snprintf(b, sizeof(b), "%s · %d / %d", T(title_txt), n, total);
  else snprintf(b, sizeof(b), "%d / %d", n, total);
  lv_obj_t *h = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(h, C_BG, 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(h, 60, 0);
  lv_obj_set_style_pad_top(h, 30, 0);
  lv_obj_set_style_pad_bottom(h, list ? 10 : 4, 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 0);
  if (title_txt && !list) {
    lv_obj_t *t = ui_label(s, title_txt, &font_sg_24, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 58);
  }
  // Wischen nach rechts = zurück (Übungsseiten werten das Wischen selbst aus)
  if (s_step > 0 && !is_tut(s_step)) ui_set_back(wz_prev_page);
}

static lv_obj_t *s_next_btn;  // rechter Pfeil der aktuellen Seite (Übung: erst gedämpft)

static lv_obj_t *arrow_btn(lv_obj_t *s, const char *icon, bool primary, int dx) {
  lv_obj_t *b = ui_btn(s, "", primary ? BTN_PRIMARY : BTN_NORMAL);
  lv_obj_set_size(b, NAV_BTN, NAV_BTN);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_t *l = lv_obj_get_child(b, 0);
  lv_label_set_text_static(l, icon);
  lv_obj_set_style_text_font(l, &font_icons_26, 0);
  lv_obj_center(l);
  // 45° unterhalb der Mitte: Abstand NAV_R vom Mittelpunkt
  lv_obj_align(b, LV_ALIGN_CENTER, dx * NAV_R * 707 / 1000, NAV_R * 707 / 1000);
  lv_obj_set_ext_click_area(b, 8);
  return b;
}

// Rechten Pfeil hervorheben (Übung geschafft) oder dämpfen (noch offen)
static void next_btn_ready(bool ready) {
  if (!s_next_btn) return;
  lv_obj_set_style_bg_color(s_next_btn, ready ? C_PRIMARY : C_SURFACE, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(s_next_btn, 0), ready ? C_ON_PRIMARY : C_MUTED, 0);
}

// list: Listen scrollen unter einer Abdeckung hindurch; next: Aktion des rechten Pfeils
static void nav(lv_obj_t *s, bool list, lv_event_cb_t next, bool ready = true) {
  if (list) {  // Einträge verschwinden unten hinter dem Bereich der Knöpfe
    lv_obj_t *cover = ui_box(s);
    lv_obj_set_size(cover, SCREEN_SIZE, SCREEN_SIZE / 2 - NAV_R * 707 / 1000 + NAV_BTN / 2 + 10);
    lv_obj_set_style_bg_color(cover, C_BG, 0);
    lv_obj_set_style_bg_opa(cover, LV_OPA_COVER, 0);
    lv_obj_align(cover, LV_ALIGN_BOTTOM_MID, 0, 0);
  }
  lv_obj_t *ring = ui_ring(s, 396);
  lv_obj_set_style_arc_width(ring, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, 6, LV_PART_INDICATOR);
  lv_arc_set_value(ring, progress(s_step));

  lv_obj_t *bk = arrow_btn(s, ICON_ZURUECK, false, -1);
  if (s_step == 0) {  // im ersten Schritt gibt es kein Zurück: nur angedeutet
    lv_obj_set_style_bg_color(bk, C_BG, 0);
    lv_obj_set_style_border_color(bk, C_TRACK, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(bk, 0), C_TRACK, 0);
    lv_obj_clear_flag(bk, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_add_event_cb(bk, back_cb, LV_EVENT_CLICKED, NULL);
  }
  int st = s_step + 1;
  while (st < WZ_COUNT && !step_used(st)) st++;
  bool last = st >= WZ_COUNT;
  s_next_btn = arrow_btn(s, last ? ICON_HAKEN : ICON_WEITER, true, 1);
  lv_obj_add_event_cb(s_next_btn, next ? next : next_cb, LV_EVENT_CLICKED, NULL);
  next_btn_ready(ready);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { forget(&s_next_btn, e); }, LV_EVENT_DELETE, NULL);
}

// Mittlerer Eintrag einer gebogenen Liste (für den rechten Pfeil)
static int list_center(lv_obj_t *list) {
  lv_area_t la;
  lv_obj_get_coords(list, &la);
  int cy = (la.y1 + la.y2) / 2, best = 0, bd = 1 << 30;
  for (int i = 0; i < (int)lv_obj_get_child_cnt(list); i++) {
    lv_area_t a;
    lv_obj_get_coords(lv_obj_get_child(list, i), &a);
    int d = (a.y1 + a.y2) / 2 - cy;
    if (d < 0) d = -d;
    if (d < bd) {
      bd = d;
      best = i;
    }
  }
  return best;
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
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 58);
  lv_obj_t *de = big_btn(s, "Deutsch", g_set.lang == LANG_DE, -40);
  lv_obj_add_event_cb(de, lang_cb, LV_EVENT_CLICKED, (void *)(intptr_t)LANG_DE);
  lv_obj_t *en = big_btn(s, "English", g_set.lang == LANG_EN, 34);
  lv_obj_add_event_cb(en, lang_cb, LV_EVENT_CLICKED, (void *)(intptr_t)LANG_EN);
  // Beschriftungen nicht übersetzen
  lv_label_set_text_static(lv_obj_get_child(de, 0), "Deutsch");
  lv_label_set_text_static(lv_obj_get_child(en, 0), "English");

  // Alternativ alles am Handy einrichten (startet die Weboberfläche mit QR-Code),
  // zwischen den beiden Pfeilen
  lv_obj_t *hd = ui_label(s, "Per Handy", &font_sg_18, C_ACCENT);
  lv_obj_add_flag(hd, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(hd, 16);
  lv_obj_align(hd, LV_ALIGN_CENTER, 0, NAV_R * 707 / 1000);
  lv_obj_add_event_cb(hd, [](lv_event_t *e) {
    sound_play(SND_CLICK);
    ui_switch_page(page_web_quick_create());
  }, LV_EVENT_CLICKED, NULL);
  // rechter Pfeil: Sprache behalten
  nav(s, false, NULL);
  return s;
}

// ---------- 2. Profil ----------
static const char *const PROF_NAMES[PROFILE_COUNT] = { "Standard", "Küche", "Werkstatt", "Einfach" };
static const char *const PROF_SUB[PROFILE_COUNT] = { "alle Modi", "Rezept, Timer, Töpfe", "Zählen, Spule, Porto",
                                                     "nur Wiegen, groß, mit Ansage" };

static lv_obj_t *s_list;  // gebogene Liste der aktuellen Seite

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
  lv_obj_align(list, LV_ALIGN_CENTER, 0, LIST_SHIFT);  // Platz für die Pfeile unten
  lv_obj_scroll_to_view(lv_obj_get_child(list, g_set.profile), LV_ANIM_OFF);
  s_list = list;
  frame(s, "Profil", true);
  // rechter Pfeil übernimmt den Eintrag in der Mitte
  nav(s, true, [](lv_event_t *e) { profile_cb(list_center(s_list)); });
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
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 94);
  for (int i = 0; i < 3; i++) {
    lv_obj_t *row = ui_box(s);
    lv_obj_set_size(row, 270, 46);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -50 + i * 48);
    lv_obj_add_event_cb(row, cat_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *l = ui_label(row, NAMES[i], &font_sg_18, C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
    cat_val[i] = ui_label(row, "", &font_sg_18, C_VALUE);
    lv_obj_align(cat_val[i], LV_ALIGN_RIGHT_MID, -4, 0);
  }
  cat_show();
  nav(s, false, NULL);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    if (cat_val[0] && lv_obj_get_screen(cat_val[0]) == lv_event_get_target(e)) memset(cat_val, 0, sizeof(cat_val));
  }, LV_EVENT_DELETE, NULL);
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
  lv_obj_align(list, LV_ALIGN_CENTER, 0, LIST_SHIFT);
  lv_obj_scroll_to_view(lv_obj_get_child(list, cur), LV_ANIM_OFF);
  s_list = list;
  frame(s, "Farbe", true);
  nav(s, true, [](lv_event_t *e) { color_cb(list_center(s_list)); });
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
  lv_obj_align(l, LV_ALIGN_CENTER, 0, -84);
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
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 28);
  lv_obj_add_event_cb(row, speak_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *rl = ui_label(row, "Gewicht ansagen", &font_sg_18, C_TEXT);
  lv_obj_align(rl, LV_ALIGN_LEFT_MID, 4, 0);
  snd_speak = ui_label(row, "", &font_sg_18, C_VALUE);
  lv_obj_align(snd_speak, LV_ALIGN_RIGHT_MID, -4, 0);
  snd_show();
  nav(s, false, NULL);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    forget(&snd_vol, e);
    forget(&snd_speak, e);
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
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -60);
  wl_state = ui_label(s, "", &font_sg_18, C_ACCENT);
  lv_obj_align(wl_state, LV_ALIGN_CENTER, 0, -6);

  lv_obj_t *set = ui_btn(s, "Einrichten", BTN_NORMAL);
  lv_obj_align(set, LV_ALIGN_CENTER, 0, 46);
  lv_obj_add_event_cb(set, [](lv_event_t *e) {
    page_setup_back_override(wz_current);  // „Fertig“ auf der WLAN-Seite kommt hierher zurück
    ui_switch_page(page_wlan_setup_create());
  }, LV_EVENT_CLICKED, NULL);
  nav(s, false, NULL);
  ui_page_timer(s, wl_timer_cb, 500);
  wl_timer_cb(NULL);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { forget(&wl_state, e); }, LV_EVENT_DELETE, NULL);
  return s;
}

// ============================================================
//  Probier es aus: Bedienung zum Mitmachen
//  Der rechte Pfeil ist gedämpft, bis die Übung geklappt hat
//  (antippen überspringt sie trotzdem).
// ============================================================
static void anim_translate_x(void *o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *)o, v, 0); }
static void anim_border_opa(void *o, int32_t v) { lv_obj_set_style_border_opa((lv_obj_t *)o, (lv_opa_t)v, 0); }

// Hand mit Leuchtspur, die immer wieder in Wischrichtung fährt (dir -1 = nach links)
static lv_obj_t *swipe_demo(lv_obj_t *s, int dir, lv_coord_t y, int amp) {
  lv_obj_t *c = ui_box(s);
  lv_obj_set_size(c, 210, 96);
  lv_obj_align(c, LV_ALIGN_CENTER, 0, y);
  lv_obj_t *trail = ui_box(c);
  lv_obj_set_size(trail, 120, 4);
  lv_obj_set_style_radius(trail, 2, 0);
  lv_obj_set_style_bg_opa(trail, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(trail, C_ACCENT, 0);
  lv_obj_set_style_bg_grad_color(trail, C_BG, 0);
  lv_obj_set_style_bg_grad_dir(trail, LV_GRAD_DIR_HOR, 0);
  lv_obj_t *dot = ui_box(c);
  lv_obj_set_size(dot, 22, 22);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dot, C_ACCENT, 0);
  lv_obj_t *hand = ui_label(c, ICON80_HAND, &font_icons_80, C_TEXT);
  if (dir < 0) {  // Spur zeigt nach hinten (rechts), Punkt vorne links
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 0, 9);
    lv_obj_align(trail, LV_ALIGN_TOP_LEFT, 11, 18);
    lv_obj_align(hand, LV_ALIGN_TOP_RIGHT, 0, 14);
  } else {
    lv_obj_set_style_bg_grad_dir(trail, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_color(trail, C_BG, 0);
    lv_obj_set_style_bg_grad_color(trail, C_ACCENT, 0);
    lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, 9);
    lv_obj_align(trail, LV_ALIGN_TOP_RIGHT, -11, 18);
    lv_obj_align(hand, LV_ALIGN_TOP_LEFT, 0, 14);
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, c);
  lv_anim_set_exec_cb(&a, anim_translate_x);
  lv_anim_set_values(&a, -dir * amp, dir * amp);
  lv_anim_set_time(&a, 1100);
  lv_anim_set_repeat_delay(&a, 500);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);
  return c;
}

// pulsierender Rahmen um das Übungsziel
static void pulse(lv_obj_t *o) {
  lv_obj_set_style_border_width(o, 3, 0);
  lv_obj_set_style_border_color(o, C_ACCENT, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, anim_border_opa);
  lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
  lv_anim_set_time(&a, 700);
  lv_anim_set_playback_time(&a, 700);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}

static lv_obj_t *tut_sub;  // Untertitel, wechselt bei Erfolg

static void tut_done(const char *msg) {
  sound_play(SND_SAVE);
  if (tut_sub && msg) {
    lv_label_set_text(tut_sub, msg);
    lv_obj_set_style_text_color(tut_sub, C_ACCENT, 0);
  }
  next_btn_ready(true);
}

static lv_obj_t *tut_page(const char *title, const char *sub) {
  lv_obj_t *s = ui_screen_create();
  frame(s, title, false);
  tut_sub = ui_label(s, sub, &font_sg_14, C_MUTED);
  lv_label_set_long_mode(tut_sub, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(tut_sub, 280);
  lv_obj_align(tut_sub, LV_ALIGN_TOP_MID, 0, 94);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { forget(&tut_sub, e); }, LV_EVENT_DELETE, NULL);
  return s;
}

// Wischen auf den Übungsseiten: Aufgabe erfüllen oder zurück
static void tut_gesture_cb(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (s_step == WZ_TUT1 && dir == LV_DIR_LEFT) {  // geschafft: weiter zu den „Modi“
    sound_play(SND_SAVE);
    wz_go(WZ_TUT2);
    return;
  }
  if (s_step == WZ_TUT2 && dir == LV_DIR_RIGHT) {  // geschafft: zurück zur „Wiegeseite“
    sound_play(SND_SAVE);
    s_step = WZ_TUT3;
    wz_load(wz_page(WZ_TUT3), false);
    return;
  }
  if (dir == LV_DIR_RIGHT && s_step != WZ_TUT2) back_cb(e);
}

static lv_obj_t *page_tut_left() {
  lv_obj_t *s = tut_page("Wische nach links", "So kommst du zu den Modi und zum System");
  swipe_demo(s, -1, 6, 50);
  lv_obj_t *h = ui_label(s, "Die Hand zeigt die Bewegung", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 62);
  lv_obj_add_event_cb(s, tut_gesture_cb, LV_EVENT_GESTURE, NULL);
  nav(s, false, NULL, false);
  return s;
}

// So sieht die Modi-Seite aus; Aufgabe: nach rechts zurück
static lv_obj_t *page_tut_right() {
  lv_obj_t *s = ui_screen_create();
  frame(s, NULL, false);
  lv_obj_t *bub = ui_box(s);
  lv_obj_set_size(bub, 224, LV_SIZE_CONTENT);  // schmal genug für den Kreis oben
  lv_obj_set_style_bg_opa(bub, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bub, C_TEXT, 0);
  lv_obj_set_style_radius(bub, 14, 0);
  lv_obj_set_style_pad_ver(bub, 8, 0);
  lv_obj_align(bub, LV_ALIGN_TOP_MID, 0, 58);
  tut_sub = ui_label(bub, "Das sind die Modi. Jetzt nach rechts zurück zur Wiegeseite", &font_sg_14, C_BG);
  lv_label_set_long_mode(tut_sub, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(tut_sub, 204);
  lv_obj_set_style_text_align(tut_sub, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(tut_sub);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { forget(&tut_sub, e); }, LV_EVENT_DELETE, NULL);

  // Vorschau der Modi-Liste (nur zum Ansehen)
  lv_obj_t *pill = ui_box(s);
  lv_obj_set_size(pill, 250, 54);
  lv_obj_set_style_radius(pill, 27, 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(pill, C_PRIMARY, 0);
  lv_obj_align(pill, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t *pi = ui_label(pill, ICON_KUECHE, &font_icons_26, C_ON_PRIMARY);
  lv_obj_align(pi, LV_ALIGN_LEFT_MID, 22, 0);
  lv_obj_t *pt = ui_label(pill, "Küche", &font_sg_24, C_ON_PRIMARY);
  lv_obj_align(pt, LV_ALIGN_CENTER, 12, 0);
  swipe_demo(s, 1, 26, 35);
  lv_obj_add_event_cb(s, tut_gesture_cb, LV_EVENT_GESTURE, NULL);
  nav(s, false, NULL, false);
  return s;
}

static lv_obj_t *page_tut_hold() {
  lv_obj_t *s = tut_page("Halte die Zahl gedrückt", "Lange drücken schaltet die Präzision um");
  lv_obj_t *box = ui_box(s);
  lv_obj_set_size(box, 230, 112);
  lv_obj_set_style_radius(box, 56, 0);
  lv_obj_align(box, LV_ALIGN_CENTER, 0, 18);
  lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *n = ui_label(box, "0,0", &font_sg_80, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, -16, 0);
  lv_obj_t *u = ui_label(box, "g", &font_sg_34, C_MUTED);
  lv_obj_align_to(u, n, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -10);
  pulse(box);
  lv_obj_add_event_cb(box, [](lv_event_t *e) {
    lv_obj_t *b = lv_event_get_target(e);
    lv_anim_del(b, anim_border_opa);
    lv_obj_set_style_border_opa(b, LV_OPA_COVER, 0);
    tut_done("Geschafft. So schaltest du die Präzision ein und aus.");
  }, LV_EVENT_LONG_PRESSED, NULL);
  lv_obj_add_event_cb(s, tut_gesture_cb, LV_EVENT_GESTURE, NULL);
  nav(s, false, NULL, false);
  return s;
}

// Profil Einfach: obere Fläche = ansagen, untere = Tara (verkleinerte Vorschau)
static lv_obj_t *page_tut_simple(bool bottom) {
  lv_obj_t *s = tut_page(bottom ? "Tippe unten auf TARA" : "Tippe oben",
                         bottom ? "Die Waage stellt auf null" : "Die Waage sagt das Gewicht an");
  lv_obj_t *top = ui_box(s);
  lv_obj_set_size(top, 210, 84);
  lv_obj_set_style_radius(top, 22, 0);
  lv_obj_align(top, LV_ALIGN_CENTER, 0, -18);
  lv_obj_t *w = ui_label(top, "250 g", &font_sg_34, C_TEXT);
  lv_obj_center(w);
  lv_obj_t *bot = ui_box(s);
  lv_obj_set_size(bot, 210, 54);
  lv_obj_set_style_radius(bot, 22, 0);
  lv_obj_set_style_bg_opa(bot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bot, lv_color_mix(C_ACCENT, C_BG, 58), 0);
  lv_obj_align(bot, LV_ALIGN_CENTER, 0, 52);
  lv_obj_t *t = ui_label(bot, "TARA", &font_sg_24, C_ACCENT);
  lv_obj_center(t);
  lv_obj_t *target = bottom ? bot : top;
  lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
  pulse(target);
  lv_obj_add_event_cb(target, [](lv_event_t *e) {
    lv_obj_t *b = lv_event_get_target(e);
    lv_anim_del(b, anim_border_opa);
    lv_obj_set_style_border_opa(b, LV_OPA_COVER, 0);
    if (s_step == WZ_TUT1) {
      sound_speak_weight_now(250.0f, 0);
      tut_done("Richtig. Oben tippen sagt das Gewicht an.");
    } else {
      tut_done("Richtig. Unten tippen stellt auf null.");
    }
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(s, tut_gesture_cb, LV_EVENT_GESTURE, NULL);
  nav(s, false, NULL, false);
  return s;
}

static lv_obj_t *wz_page(int step) {
  switch (step) {
    case WZ_PROFILE: return page_profile();
    case WZ_CATS: return page_cats();
    case WZ_COLOR: return page_color();
    case WZ_SOUND: return page_sound();
    case WZ_WLAN: return page_wlan();
    case WZ_TUT1: return g_set.profile == PROFILE_EINFACH ? page_tut_simple(false) : page_tut_left();
    case WZ_TUT2: return g_set.profile == PROFILE_EINFACH ? page_tut_simple(true) : page_tut_right();
    case WZ_TUT3: return page_tut_hold();
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
