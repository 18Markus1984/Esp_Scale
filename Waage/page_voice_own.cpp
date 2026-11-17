// ============================================================
//  Setup -> Ton -> Eigene Stimme
//
//  Geführt Schritt für Schritt: die Seite zeigt, wann die Waage die Ansage
//  sagt und einen Vorschlag, was man sagen kann. Knopf gedrückt halten und
//  sprechen, loslassen – die Aufnahme wird gleich vorgespielt. „Nochmal“
//  oder „Passt“ und weiter zur nächsten. Erst die 7 wichtigsten Ansagen,
//  danach auf Wunsch der Rest. Was man sagt, ist frei (z. B. „Hallo
//  Schatz, jetzt kannst du tara“). Paket und Verwaltung: voice_own.cpp.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "voice.h"
#include "voice_own.h"
#include "sound.h"
#include "storage.h"
#include "settings.h"
#include "data.h"
#include "hal.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int o_slot = 0;
static int o_done_group = 0;
static lv_obj_t *o_ring, *o_state, *o_btn, *o_ic, *o_hold, *o_level, *o_play, *o_next;
static int o_prev_state = -9;
static bool o_marked = false;
static bool o_holding = false;      // Knopf gedrückt
static uint32_t o_release_ms = 0;   // losgelassen: kurz nachlaufen lassen, dann beenden

static lv_obj_t *page_own_rec_create();
static lv_obj_t *page_own_done_create();

static void use_this() {
  strncpy(g_set.voice, own_pack(), sizeof(g_set.voice) - 1);
  g_set.voice[sizeof(g_set.voice) - 1] = 0;
  settings_save();
  sound_voice_set(g_set.voice);
}

// erste noch nicht aufgenommene Ansage (ab from), -1 = alle eigene
static int first_open(int from) {
  const own_slot_t *sl;
  int n = own_slots(&sl);
  for (int i = from; i < n; i++)
    if (!own_is_mine(sl[i].file)) return i;
  for (int i = 0; i < from && i < n; i++)
    if (!own_is_mine(sl[i].file)) return i;
  return -1;
}

static void group_pos(int slot, int *pos, int *all) {
  const own_slot_t *sl;
  int n = own_slots(&sl);
  *pos = 0;
  *all = 0;
  for (int i = 0; i < n; i++)
    if (sl[i].group == sl[slot].group) {
      if (i <= slot) (*pos)++;
      (*all)++;
    }
}

static int group_mine(int g, int *all) {
  const own_slot_t *sl;
  int n = own_slots(&sl), m = 0;
  *all = 0;
  for (int i = 0; i < n; i++)
    if (sl[i].group == g) {
      (*all)++;
      if (own_is_mine(sl[i].file)) m++;
    }
  return m;
}

// ------------------------------------------------------------
//  Übersicht (Weiter aufnehmen, Verwenden, Gruppen) und Liste einer Gruppe
// ------------------------------------------------------------
static int o_group = -1;  // -1 = Übersicht
static ui_list_item_t o_items[52];
static int o_map[52];
static char (*o_subs)[48] = NULL;  // im PSRAM, beim ersten Aufbau

static lv_obj_t *page_own_group_create();

static void top_pick_cb(int index) {
  if (index == 0) {  // weiter aufnehmen
    int k = first_open(0);
    if (k < 0) {
      o_group = 0;
      ui_switch_page(page_own_group_create());
      return;
    }
    o_slot = k;
    ui_switch_page(page_own_rec_create());
    return;
  }
  if (index == 1) {  // Stimme verwenden
    use_this();
    sound_play(SND_SAVE);
    ui_switch_page(page_voice_own_create());
    return;
  }
  o_group = index - 2;
  ui_switch_page(page_own_group_create());
}

static void group_pick_cb(int index) {
  o_slot = o_map[index];
  ui_switch_page(page_own_rec_create());
}

static void create_cb(lv_event_t *e) {
  if (!own_create("")) {  // legt nur den Ordner an, kopiert nichts
    sound_play(SND_WARN);
    ui_switch_page(page_voice_own_create());
    return;
  }
  sound_play(SND_SAVE);
  o_slot = 0;  // gleich mit der ersten Ansage los
  ui_switch_page(page_own_rec_create());
}

static void back_cb(lv_event_t *e) { ui_switch_page(page_setup_create()); }

static void list_title(lv_obj_t *s, const char *txt) {
  lv_obj_t *t = ui_label(s, txt, &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
}

static lv_obj_t *wrap_label(lv_obj_t *s, const char *txt, const lv_font_t *f, lv_color_t c, int w) {
  lv_obj_t *l = ui_label(s, txt, f, c);
  lv_obj_set_width(l, w);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

static lv_obj_t *btn_row(lv_obj_t *s, int y) {
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
  return row;
}

lv_obj_t *page_voice_own_create() {
  if (!o_subs) o_subs = (char (*)[48])big_calloc(52, 48);
  lv_obj_t *s = ui_screen_create();
  if (!storage_ok()) {
    lv_obj_t *l = ui_label(s, "Keine SD-Karte", &font_sg_18, C_MUTED);
    lv_obj_center(l);
    return s;
  }
  if (!own_exists()) {  // Einstieg: kurz erklären, dann geht es sofort los
    lv_obj_t *t = ui_label(s, "Eigene Stimme", &font_sg_24, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_t *ic = ui_label(s, ICON_MIKRO, &font_icons_26, C_ACCENT);
    lv_obj_align(ic, LV_ALIGN_CENTER, 0, -108);
    lv_obj_t *h = wrap_label(s, "Die Waage zeigt dir jede Ansage mit einem Vorschlag. Knopf halten, sprechen, loslassen.",
                             &font_sg_18, C_TEXT2, 292);
    lv_obj_align(h, LV_ALIGN_CENTER, 0, -42);
    lv_obj_t *h2 = wrap_label(s, "Eigene Sätze gehen auch. Erst die 7 wichtigsten, der Rest später.", &font_sg_14, C_MUTED, 260);
    lv_obj_align(h2, LV_ALIGN_CENTER, 0, 44);
    lv_obj_t *row = btn_row(s, 112);
    lv_obj_t *bk = ui_btn(row, "Zurück", BTN_NORMAL);
    lv_obj_add_event_cb(bk, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *go = ui_btn(row, "Los geht's", BTN_PRIMARY);
    lv_obj_add_event_cb(go, create_cb, LV_EVENT_CLICKED, NULL);
    return s;
  }
  const own_slot_t *sl;
  int n = own_slots(&sl);
  int open = first_open(0);
  o_items[0].title = open >= 0 ? "Weiter aufnehmen" : "Alles aufgenommen";
  if (open >= 0) snprintf(o_subs[OWN_GROUPS], 48, T("als Nächstes: %s"), sl[open].text);
  else snprintf(o_subs[OWN_GROUPS], 48, "%s", T("Ansagen ansehen und ändern"));
  o_items[0].sub = o_subs[OWN_GROUPS];
  o_items[0].icon = NULL;
  bool in_use = strcmp(g_set.voice, own_pack()) == 0;
  o_items[1].title = in_use ? "Eingeschaltet" : "Einschalten";
  o_items[1].sub = in_use ? "Die Waage spricht mit deiner Stimme" : "statt der bisherigen Stimme";
  o_items[1].icon = NULL;
  for (int g = 0; g < OWN_GROUPS; g++) {
    int all, mine = group_mine(g, &all);
    snprintf(o_subs[g], 48, T("%d von %d eigene"), mine, all);
    o_items[g + 2].title = own_group_name(g);
    o_items[g + 2].sub = o_subs[g];
    o_items[g + 2].icon = NULL;
  }
  lv_obj_t *list = ui_curved_list(s, o_items, OWN_GROUPS + 2, top_pick_cb);
  if (o_group >= 0) lv_obj_scroll_to_view(lv_obj_get_child(list, o_group + 2), LV_ANIM_OFF);
  char b[40];
  snprintf(b, sizeof(b), T("Eigene Stimme · %d/%d"), own_count_mine(), n);
  list_title(s, b);
  o_group = -1;
  return s;
}

static lv_obj_t *page_own_group_create() {
  lv_obj_t *s = ui_screen_create();
  const own_slot_t *sl;
  int n = own_slots(&sl), k = 0, sel = 0;
  for (int i = 0; i < n && k < 52; i++) {
    if (sl[i].group != o_group) continue;
    if (i == o_slot) sel = k;
    o_map[k] = i;
    o_items[k].title = sl[i].text;
    o_items[k].sub = own_is_mine(sl[i].file) ? T("eigene Aufnahme") : T("noch nicht aufgenommen");
    o_items[k].icon = own_is_mine(sl[i].file) ? ICON_MIKRO : NULL;
    k++;
  }
  lv_obj_t *list = ui_curved_list(s, o_items, k, group_pick_cb);
  if (k) lv_obj_scroll_to_view(lv_obj_get_child(list, sel), LV_ANIM_OFF);
  list_title(s, T(own_group_name(o_group)));
  return s;
}

// ------------------------------------------------------------
//  Eine Ansage aufnehmen: halten, sprechen, loslassen
// ------------------------------------------------------------
static void show_state(const char *txt, lv_color_t c) {
  ui_label_update(o_state, txt);
  lv_obj_set_style_text_color(o_state, c, 0);
}

static void update_buttons() {
  const own_slot_t *sl;
  own_slots(&sl);
  bool mine = own_is_mine(sl[o_slot].file);
  if (mine) lv_obj_clear_state(o_play, LV_STATE_DISABLED);
  else lv_obj_add_state(o_play, LV_STATE_DISABLED);
  lv_obj_set_style_text_color(lv_obj_get_child(o_play, 0), mine ? C_TEXT : C_FAINT, 0);  // noch nichts zum Anhören
  // vor der Aufnahme ist der Mikrofonknopf das einzige grüne Element, danach "Passt ›"
  lv_obj_t *nl = lv_obj_get_child(o_next, 0);
  lv_label_set_text(nl, mine ? T("Passt ›") : T("Später"));
  lv_obj_set_style_bg_color(o_next, mine ? C_PRIMARY : C_SURFACE, 0);
  lv_obj_set_style_border_width(o_next, mine ? 0 : 2, 0);
  lv_obj_set_style_border_color(o_next, C_BORDER, 0);
  lv_obj_set_style_text_color(nl, mine ? C_ON_PRIMARY : C_TEXT, 0);
}

static void o_timer_cb(lv_timer_t *t) {
  const own_slot_t *sl;
  own_slots(&sl);
  int st = voice_rec_state();
  // losgelassen: 300 ms nachlaufen lassen (das letzte Wort nicht abschneiden)
  if (!o_holding && o_release_ms && hal_millis() - o_release_ms > 300 && st == 2) {
    o_release_ms = 0;
    voice_rec_stop();
  }
  int lv = (int)(voice_level() * 4.0f * 200.0f);
  if (lv > 200) lv = 200;
  if (lv < 4) lv = 4;
  lv_obj_set_width(o_level, lv);
  if (st == 2) lv_obj_clear_flag(o_level, LV_OBJ_FLAG_HIDDEN);  // Pegel nur während der Aufnahme
  else lv_obj_add_flag(o_level, LV_OBJ_FLAG_HIDDEN);
  int v = st == 2 ? (int)(voice_rec_progress() * 1000.0f) : 0;
  if (lv_arc_get_value(o_ring) != v) lv_arc_set_value(o_ring, v);
  if (st == o_prev_state) return;
  int was = o_prev_state;
  o_prev_state = st;
  if (st == 1 || st == 2) show_state("Sprich jetzt … dann loslassen", C_ACCENT);
  else if (st == 3) show_state("speichert …", C_MUTED);
  else if (st == 4) {
    show_state("Hör mal rein – passt es?", C_ACCENT);
    if (!o_marked) {
      o_marked = true;
      own_mark(sl[o_slot].file);
      sound_preview(own_pack(), sl[o_slot].file);  // gleich einmal vorspielen
      update_buttons();
    }
  } else if (st == -2) show_state("Nichts gehört – länger halten, lauter sprechen", C_DANGER);
  else if (st < 0) show_state("Nicht gespeichert (SD-Karte?)", C_DANGER);
  else if (was == 4 || was == -2) {
    // Meldung stehen lassen, bis der Nutzer etwas tut
  } else if (own_is_mine(sl[o_slot].file)) show_state("Halten für eine neue Aufnahme", C_MUTED);
  else show_state("Gedrückt halten und sprechen", C_MUTED);
  // Knopf: während der Aufnahme weiß (gut sichtbar "läuft"), sonst grün
  bool on = st == 1 || st == 2;
  lv_obj_set_style_bg_color(o_btn, on ? C_TEXT : C_PRIMARY, 0);
  lv_obj_set_style_text_color(o_ic, on ? C_BG : C_ON_PRIMARY, 0);
  lv_obj_set_style_text_color(o_hold, on ? C_BG : C_ON_PRIMARY, 0);
  ui_label_update(o_hold, on ? "läuft" : "halten");
  if (st == 3) lv_obj_add_state(o_btn, LV_STATE_DISABLED);
  else lv_obj_clear_state(o_btn, LV_STATE_DISABLED);
}

static void o_press_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    int st = voice_rec_state();
    if (st == 1 || st == 2 || st == 3) return;
    const own_slot_t *sl;
    own_slots(&sl);
    char path[96];
    own_path(path, sizeof(path), sl[o_slot].file);
    sound_speak_stop();          // Vorhören abbrechen
    sound_play_tone(SND_CLICK);  // kurzer Klick, die Aufnahme beginnt gleich danach
    o_marked = false;
    o_release_ms = 0;
    if (!voice_rec_file(path, sl[o_slot].max_s * 1000)) {
      sound_play(SND_WARN);
      show_state("Mikrofon nicht bereit", C_DANGER);
      return;
    }
    o_holding = true;
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    if (!o_holding) return;
    o_holding = false;
    o_release_ms = hal_millis();
    if (!o_release_ms) o_release_ms = 1;
  }
}

static void o_play_cb(lv_event_t *e) {
  const own_slot_t *sl;
  own_slots(&sl);
  sound_preview(own_pack(), sl[o_slot].file);
}

// weiter: nächste Ansage der Gruppe, am Ende der Gruppe die Abschlussseite
static void o_next_cb(lv_event_t *e) {
  const own_slot_t *sl;
  int n = own_slots(&sl);
  int g = sl[o_slot].group;
  if (o_slot + 1 < n && sl[o_slot + 1].group == g) {
    o_slot++;
    ui_switch_page(page_own_rec_create());
  } else {
    o_done_group = g;
    ui_switch_page(page_own_done_create());
  }
}

static void o_list_cb(lv_event_t *e) { ui_switch_page(page_voice_own_create()); }

static void session_del_cb(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
    if (o_holding || voice_rec_state() == 2) voice_rec_stop();
    o_holding = false;
    voice_rec_session(false);
  }
}

static lv_obj_t *page_own_rec_create() {
  const own_slot_t *sl;
  int n = own_slots(&sl);
  if (o_slot < 0 || o_slot >= n) o_slot = 0;
  const own_slot_t *it = &sl[o_slot];
  lv_obj_t *s = ui_screen_create();
  voice_rec_session(true);  // Mikrofon bereithalten
  lv_obj_add_event_cb(s, session_del_cb, LV_EVENT_DELETE, NULL);
  o_ring = ui_ring(s, 396);
  lv_obj_set_style_arc_color(o_ring, C_ACCENT, LV_PART_INDICATOR);

  int pos, all;
  group_pos(o_slot, &pos, &all);
  char b[64];
  snprintf(b, sizeof(b), T("%s · %d von %d"), T(own_group_name(it->group)), pos, all);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 44);

  // wann sagt die Waage das?
  char when[96];
  snprintf(when, sizeof(when), "%s", it->when);
  if (when[0] && islower((unsigned char)when[0])) when[0] = toupper((unsigned char)when[0]);
  lv_obj_t *w = wrap_label(s, when, &font_sg_14, C_TEXT2, 250);
  lv_obj_align(w, LV_ALIGN_TOP_MID, 0, 64);

  lv_obj_t *say = ui_label(s, "Sag zum Beispiel", &font_sg_14, C_FAINT);
  lv_obj_align(say, LV_ALIGN_CENTER, 0, -86);
  int len = strlen(it->text);
  const lv_font_t *f = len <= 14 ? &font_sg_34 : (len <= 30 ? &font_sg_24 : &font_sg_18);
  lv_obj_t *tx = wrap_label(s, it->text, f, C_TEXT, 310);
  lv_obj_align(tx, LV_ALIGN_CENTER, 0, -52);
  lv_obj_t *own = ui_label(s, "oder mit eigenen Worten", &font_sg_14, C_FAINT);
  lv_obj_align(own, LV_ALIGN_CENTER, 0, -20);

  o_btn = ui_btn(s, "", BTN_PRIMARY);  // runder Knopf: halten = aufnehmen
  lv_obj_set_size(o_btn, 84, 84);
  lv_obj_set_style_radius(o_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_hor(o_btn, 0, 0);
  lv_obj_align(o_btn, LV_ALIGN_CENTER, 0, 30);
  lv_obj_add_event_cb(o_btn, o_press_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(o_btn, o_press_cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(o_btn, o_press_cb, LV_EVENT_PRESS_LOST, NULL);
  lv_obj_clear_flag(o_btn, LV_OBJ_FLAG_SCROLL_CHAIN);
  o_ic = lv_obj_get_child(o_btn, 0);
  lv_label_set_text(o_ic, ICON_MIKRO);
  lv_obj_set_style_text_font(o_ic, &font_icons_26, 0);
  lv_obj_align(o_ic, LV_ALIGN_CENTER, 0, -9);
  o_hold = ui_label(o_btn, "halten", &font_sg_14, C_ON_PRIMARY);  // sagt, dass man halten muss
  lv_obj_align(o_hold, LV_ALIGN_CENTER, 0, 16);

  o_level = ui_box(s);
  lv_obj_set_size(o_level, 4, 4);
  lv_obj_set_style_bg_color(o_level, C_ACCENT, 0);
  lv_obj_set_style_bg_opa(o_level, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o_level, 2, 0);
  lv_obj_align(o_level, LV_ALIGN_CENTER, 0, 78);

  o_state = wrap_label(s, "", &font_sg_14, C_MUTED, 260);
  lv_obj_align(o_state, LV_ALIGN_CENTER, 0, 92);

  lv_obj_t *row = btn_row(s, 130);
  lv_obj_set_style_pad_column(row, 12, 0);
  o_play = ui_btn(row, "Anhören", BTN_NORMAL);
  lv_obj_set_width(o_play, 112);
  lv_obj_set_style_pad_hor(o_play, 0, 0);
  lv_obj_add_event_cb(o_play, o_play_cb, LV_EVENT_CLICKED, NULL);
  o_next = ui_btn(row, "Später", BTN_PRIMARY);
  lv_obj_set_width(o_next, 112);
  lv_obj_set_style_pad_hor(o_next, 0, 0);
  lv_obj_add_event_cb(o_next, o_next_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ls = ui_label(s, "Übersicht", &font_sg_14, C_MUTED);  // klein unten: zurück zur Übersicht
  lv_obj_set_style_pad_hor(ls, 24, 0);
  lv_obj_set_style_pad_ver(ls, 14, 0);  // großer Tippbereich
  lv_obj_add_flag(ls, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(ls, LV_ALIGN_CENTER, 0, 176);
  lv_obj_add_event_cb(ls, o_list_cb, LV_EVENT_CLICKED, NULL);
  update_buttons();

  o_prev_state = -9;
  o_marked = true;  // ein altes "gespeichert" nicht als neue Aufnahme werten
  o_holding = false;
  o_release_ms = 0;
  ui_page_timer(s, o_timer_cb, 100);
  o_timer_cb(NULL);
  ui_keep_open(s);
  return s;
}

// ------------------------------------------------------------
//  Gruppe geschafft: verwenden, mit der nächsten Gruppe weiter oder fertig
// ------------------------------------------------------------
static void d_more_cb(lv_event_t *e) {
  const own_slot_t *sl;
  int n = own_slots(&sl);
  for (int i = 0; i < n; i++)
    if (sl[i].group == o_done_group + 1) {
      int k = i;  // erste offene der nächsten Gruppe, sonst die erste
      for (int j = i; j < n && sl[j].group == o_done_group + 1; j++)
        if (!own_is_mine(sl[j].file)) {
          k = j;
          break;
        }
      o_slot = k;
      ui_switch_page(page_own_rec_create());
      return;
    }
  ui_switch_page(page_voice_own_create());
}

static void d_use_cb(lv_event_t *e) {
  use_this();
  sound_play(SND_SAVE);
  ui_switch_page(page_own_done_create());
}

static void d_list_cb(lv_event_t *e) { ui_switch_page(page_voice_own_create()); }

static lv_obj_t *page_own_done_create() {
  lv_obj_t *s = ui_screen_create();
  int all, mine = group_mine(o_done_group, &all);
  bool in_use = strcmp(g_set.voice, own_pack()) == 0;
  bool last = o_done_group + 1 >= OWN_GROUPS;
  if (mine) {
    lv_obj_t *ic = ui_label(s, ICON36_HAKEN, &font_icons_36, C_ACCENT);
    lv_obj_align(ic, LV_ALIGN_CENTER, 0, -112);
  }
  lv_obj_t *t = wrap_label(s, T(own_group_name(o_done_group)), &font_sg_24, C_TEXT, 300);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -66);
  char b[48];
  if (mine) snprintf(b, sizeof(b), T("%d von %d selbst aufgenommen"), mine, all);
  else snprintf(b, sizeof(b), "%s", T("Noch nichts aufgenommen"));
  lv_obj_t *c = ui_label(s, b, &font_sg_18, C_TEXT2);
  lv_obj_align(c, LV_ALIGN_CENTER, 0, -30);
  const char *hint = !mine ? "Du kannst jederzeit weitermachen."
                     : in_use ? "Die Waage spricht jetzt mit deiner Stimme."
                              : "Noch spricht die bisherige Stimme.";
  lv_obj_t *h = wrap_label(s, hint, &font_sg_14, C_MUTED, 260);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 6);

  lv_obj_t *row = btn_row(s, 62);
  if (mine && !in_use) {
    lv_obj_t *u = ui_btn(row, "Einschalten", BTN_PRIMARY);
    lv_obj_add_event_cb(u, d_use_cb, LV_EVENT_CLICKED, NULL);
  }
  lv_obj_t *ls = ui_btn(row, "Fertig", mine && !in_use ? BTN_NORMAL : BTN_PRIMARY);
  lv_obj_add_event_cb(ls, d_list_cb, LV_EVENT_CLICKED, NULL);
  if (!last) {
    char nx[64];
    snprintf(nx, sizeof(nx), T("Weiter: %s"), T(own_group_name(o_done_group + 1)));
    lv_obj_t *m = ui_btn(s, nx, BTN_NORMAL);
    lv_obj_set_style_pad_hor(m, 16, 0);
    lv_obj_align(m, LV_ALIGN_CENTER, 0, 120);
    lv_obj_add_event_cb(m, d_more_cb, LV_EVENT_CLICKED, NULL);
  }
  return s;
}
