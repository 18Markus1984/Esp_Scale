// ============================================================
//  Setup -> Ton -> Eigene Stimme
//
//  Jede Ansage selbst aufnehmen: Liste aller Ansagen, antippen, nach dem
//  Ton sprechen, nochmal antippen beendet. Was man sagt, ist frei
//  (z. B. „Hallo Schatz, jetzt kannst du tara“). Danach wird die Aufnahme
//  einmal vorgespielt. Paket und Verwaltung: voice_own.cpp.
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
#include <stdio.h>
#include <string.h>

static int o_slot = 0;
static lv_obj_t *o_ring, *o_state, *o_btn, *o_ic, *o_level, *o_mine, *o_play;
static int o_prev_state = -9;
static bool o_marked = false;

static lv_obj_t *page_own_rec_create();

static void use_this() {
  strncpy(g_set.voice, own_pack(), sizeof(g_set.voice) - 1);
  g_set.voice[sizeof(g_set.voice) - 1] = 0;
  settings_save();
  sound_voice_set(g_set.voice);
}

// ------------------------------------------------------------
//  Übersicht (Verwenden + Gruppen) und Liste einer Gruppe
// ------------------------------------------------------------
static int o_group = -1;                 // -1 = Übersicht
static ui_list_item_t o_items[52];
static int o_map[52];
static char (*o_subs)[40] = NULL;  // im PSRAM, beim ersten Aufbau

static lv_obj_t *page_own_group_create();

static void top_pick_cb(int index) {
  if (index == 0) {  // Stimme verwenden
    use_this();
    sound_play(SND_SAVE);
    ui_switch_page(page_voice_own_create());
    return;
  }
  o_group = index - 1;
  ui_switch_page(page_own_group_create());
}

static void group_pick_cb(int index) {
  o_slot = o_map[index];
  ui_switch_page(page_own_rec_create());
}

static void create_cb(lv_event_t *e) {
  lv_obj_t *b = lv_event_get_target(e);
  lv_label_set_text(lv_obj_get_child(b, 0), T("kopiert …"));
  lv_refr_now(NULL);  // Kopieren dauert ein paar Sekunden
  bool ok = own_create("");
  sound_play(ok ? SND_SAVE : SND_WARN);
  ui_switch_page(page_voice_own_create());
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

lv_obj_t *page_voice_own_create() {
  if (!o_subs) o_subs = (char (*)[40])big_calloc(52, 40);
  lv_obj_t *s = ui_screen_create();
  if (!storage_ok()) {
    lv_obj_t *l = ui_label(s, "Keine SD-Karte", &font_sg_18, C_MUTED);
    lv_obj_center(l);
    return s;
  }
  if (!own_exists()) {  // erst anlegen: Vorlage = gewählte Stimme
    lv_obj_t *t = ui_label(s, "Eigene Stimme", &font_sg_24, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_t *ic = ui_label(s, ICON_MIKRO, &font_icons_26, C_ACCENT);
    lv_obj_align(ic, LV_ALIGN_CENTER, 0, -96);
    lv_obj_t *h = ui_label(s,
                           "Jede Ansage selbst aufnehmen, auch eigene Sätze wie „Hallo Schatz, jetzt kannst du tara“. "
                           "Alles, was du nicht aufnimmst, kommt aus der gewählten Stimme.",
                           &font_sg_18, C_TEXT2);
    lv_obj_set_width(h, 290);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(h, LV_ALIGN_CENTER, 0, -12);
    lv_obj_t *row = ui_box(s);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 96);
    lv_obj_t *bk = ui_btn(row, "Zurück", BTN_NORMAL);
    lv_obj_add_event_cb(bk, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *go = ui_btn(row, "Anlegen", BTN_PRIMARY);
    lv_obj_add_event_cb(go, create_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *w = ui_label(s, "Geht auch im Browser", &font_sg_14, C_FAINT);
    lv_obj_align(w, LV_ALIGN_CENTER, 0, 148);
    return s;
  }
  const own_slot_t *sl;
  int n = own_slots(&sl);
  bool in_use = strcmp(g_set.voice, own_pack()) == 0;
  o_items[0].title = in_use ? "Wird verwendet" : "Verwenden";
  o_items[0].sub = in_use ? "Eigene Stimme ist aktiv" : "statt der bisherigen Stimme";
  o_items[0].icon = NULL;
  for (int g = 0; g < OWN_GROUPS; g++) {
    int all = 0, mine = 0;
    for (int i = 0; i < n; i++)
      if (sl[i].group == g) {
        all++;
        if (own_is_mine(sl[i].file)) mine++;
      }
    snprintf(o_subs[g], sizeof(o_subs[g]), T("%d von %d eigene"), mine, all);
    o_items[g + 1].title = own_group_name(g);
    o_items[g + 1].sub = o_subs[g];
    o_items[g + 1].icon = mine ? ICON_MIKRO : NULL;
  }
  lv_obj_t *list = ui_curved_list(s, o_items, OWN_GROUPS + 1, top_pick_cb);
  if (o_group >= 0) lv_obj_scroll_to_view(lv_obj_get_child(list, o_group + 1), LV_ANIM_OFF);
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
    o_items[k].sub = own_is_mine(sl[i].file) ? T("eigene Aufnahme") : T("aus der Vorlage");
    o_items[k].icon = own_is_mine(sl[i].file) ? ICON_MIKRO : NULL;
    k++;
  }
  lv_obj_t *list = ui_curved_list(s, o_items, k, group_pick_cb);
  if (k) lv_obj_scroll_to_view(lv_obj_get_child(list, sel), LV_ANIM_OFF);
  list_title(s, T(own_group_name(o_group)));
  return s;
}

// ------------------------------------------------------------
//  Eine Ansage aufnehmen
// ------------------------------------------------------------
static void o_timer_cb(lv_timer_t *t) {
  const own_slot_t *sl;
  own_slots(&sl);
  int st = voice_rec_state();
  int lv = (int)(voice_level() * 4.0f * 200.0f);
  if (lv > 200) lv = 200;
  if (lv < 4) lv = 4;
  lv_obj_set_width(o_level, lv);
  int v = (int)(voice_rec_progress() * 1000.0f);
  if (lv_arc_get_value(o_ring) != v) lv_arc_set_value(o_ring, v);
  if (st == o_prev_state) return;
  o_prev_state = st;
  const char *txt = "Antippen, nach dem Ton sprechen";
  lv_color_t c = C_MUTED;
  if (st == 1) txt = "gleich …";
  else if (st == 2) {
    txt = "Jetzt sprechen – antippen zum Beenden";
    c = C_ACCENT;
  } else if (st == 3) txt = "speichert …";
  else if (st == 4) {
    txt = "Gespeichert – hör mal rein";
    c = C_ACCENT;
    if (!o_marked) {
      o_marked = true;
      own_mark(sl[o_slot].file);
      lv_obj_clear_flag(o_mine, LV_OBJ_FLAG_HIDDEN);
      sound_preview(own_pack(), sl[o_slot].file);  // gleich einmal vorspielen
    }
  } else if (st < 0) {
    txt = "Nicht gespeichert (SD-Karte?)";
    c = C_DANGER;
  }
  ui_label_update(o_state, txt);
  lv_obj_set_style_text_color(o_state, c, 0);
  bool busy = st == 1 || st == 3;
  if (busy) lv_obj_add_state(o_btn, LV_STATE_DISABLED);
  else lv_obj_clear_state(o_btn, LV_STATE_DISABLED);
  lv_label_set_text(o_ic, st == 2 ? T("Stopp") : ICON_MIKRO);
  lv_obj_set_style_text_font(o_ic, st == 2 ? &font_sg_18 : &font_icons_26, 0);
  if (st == 0) lv_arc_set_value(o_ring, 0);
}

static void o_rec_cb(lv_event_t *e) {
  int st = voice_rec_state();
  if (st == 2) {  // läuft: beenden und speichern
    sound_play(SND_CLICK);
    voice_rec_stop();
    return;
  }
  if (st == 1 || st == 3) return;
  const own_slot_t *sl;
  own_slots(&sl);
  char path[96];
  own_path(path, sizeof(path), sl[o_slot].file);
  sound_play_tone(SND_PARK);  // Piepton, die Aufnahme beginnt kurz danach
  o_marked = false;
  if (!voice_rec_file(path, sl[o_slot].max_s * 1000)) {
    sound_play(SND_WARN);
    ui_label_update(o_state, "Mikrofon nicht bereit");
  }
}

static void o_play_cb(lv_event_t *e) {
  const own_slot_t *sl;
  own_slots(&sl);
  sound_preview(own_pack(), sl[o_slot].file);
}

static void o_next_cb(lv_event_t *e) {
  const own_slot_t *sl;
  int n = own_slots(&sl);
  if (o_slot + 1 < n) {
    o_slot++;
    ui_switch_page(page_own_rec_create());
  } else {
    ui_switch_page(page_voice_own_create());
  }
}

static void o_list_cb(lv_event_t *e) {
  const own_slot_t *sl;
  own_slots(&sl);
  o_group = sl[o_slot].group;
  ui_switch_page(page_own_group_create());
}

static void session_del_cb(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) voice_rec_session(false);
}

static lv_obj_t *page_own_rec_create() {
  const own_slot_t *sl;
  int n = own_slots(&sl);
  if (o_slot < 0 || o_slot >= n) o_slot = 0;
  lv_obj_t *s = ui_screen_create();
  voice_rec_session(true);  // Mikrofon bereithalten
  lv_obj_add_event_cb(s, session_del_cb, LV_EVENT_DELETE, NULL);
  o_ring = ui_ring(s, 396);
  lv_obj_set_style_arc_color(o_ring, C_ACCENT, LV_PART_INDICATOR);

  char b[64];
  snprintf(b, sizeof(b), "%s · %d/%d", T(own_group_name(sl[o_slot].group)), o_slot + 1, n);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *w = ui_label(s, sl[o_slot].text, &font_sg_24, C_TEXT);
  lv_obj_set_width(w, 300);
  lv_label_set_long_mode(w, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(w, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(w, LV_ALIGN_CENTER, 0, -104);
  lv_obj_t *h = ui_label(s, "oder etwas Eigenes", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -72);

  o_btn = ui_btn(s, "", BTN_PRIMARY);  // runder Aufnahmeknopf im Stil der übrigen Knöpfe
  lv_obj_set_size(o_btn, 80, 80);
  lv_obj_set_style_pad_hor(o_btn, 0, 0);
  lv_obj_align(o_btn, LV_ALIGN_CENTER, 0, -14);
  lv_obj_add_event_cb(o_btn, o_rec_cb, LV_EVENT_CLICKED, NULL);
  o_ic = lv_obj_get_child(o_btn, 0);
  lv_label_set_text(o_ic, ICON_MIKRO);
  lv_obj_set_style_text_font(o_ic, &font_icons_26, 0);
  lv_obj_center(o_ic);

  o_level = ui_box(s);
  lv_obj_set_size(o_level, 4, 4);
  lv_obj_set_style_bg_color(o_level, C_ACCENT, 0);
  lv_obj_set_style_bg_opa(o_level, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o_level, 2, 0);
  lv_obj_align(o_level, LV_ALIGN_CENTER, 0, 36);

  o_state = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(o_state, LV_ALIGN_CENTER, 0, 54);
  o_mine = ui_label(s, "eigene Aufnahme", &font_sg_14, C_ACCENT);
  lv_obj_align(o_mine, LV_ALIGN_CENTER, 0, 76);
  if (!own_is_mine(sl[o_slot].file)) lv_obj_add_flag(o_mine, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 112);
  lv_obj_t *ls = ui_btn(row, "Liste", BTN_NORMAL);
  lv_obj_set_style_pad_hor(ls, 12, 0);
  lv_obj_add_event_cb(ls, o_list_cb, LV_EVENT_CLICKED, NULL);
  o_play = ui_btn(row, "Anhören", BTN_NORMAL);
  lv_obj_set_style_pad_hor(o_play, 12, 0);
  lv_obj_add_event_cb(o_play, o_play_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *nx = ui_btn(row, o_slot + 1 < n ? "Nächste" : "Fertig", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(nx, 12, 0);
  lv_obj_add_event_cb(nx, o_next_cb, LV_EVENT_CLICKED, NULL);

  o_prev_state = -9;
  o_marked = true;  // ein altes "gespeichert" nicht als neue Aufnahme werten
  ui_page_timer(s, o_timer_cb, 100);
  o_timer_cb(NULL);
  ui_keep_open(s);
  return s;
}
