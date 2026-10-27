// ============================================================
//  Hilfe zur Sprachsteuerung (siehe voice_help.h)
// ============================================================
#include "voice_help.h"
#include "voice.h"
#include "ui.h"
#include "ui_pages.h"
#include "ui_theme.h"
#include "hal.h"
#include "settings.h"
#include "i18n.h"
#include <stdio.h>
#include <string.h>

const char *const VOICE_NEXT_LABELS[] = { "Weiter", "Fertig", "Start", "Tippen", "Los",
                                          "Nächste Runde", "Übernehmen", "Mixen" };
const int VOICE_NEXT_COUNT = sizeof(VOICE_NEXT_LABELS) / sizeof(VOICE_NEXT_LABELS[0]);

#define CONFIRM_MS 1300    // so lange steht nach einem Befehl, was verstanden wurde
#define HELP_OPEN_MS 45000 // Liste aller Befehle schließt sich danach von selbst

// ---- alle Befehle (Reihenfolge = Anzeige) ----
typedef struct {
  const char *group;  // Überschrift vor dem Eintrag (NULL = gleiche Gruppe)
  const char *say;    // was man sagt (ohne Anführungszeichen)
  const char *does;   // was passiert
} vcmd_t;
static const vcmd_t CMDS[] = {
  { "Wiegen", "Tara", "auf null stellen (auch „Null“)" },
  { NULL, "Wie viel?", "das Gewicht ansagen" },
  { NULL, "Speichern", "Gewicht ins Protokoll" },
  { "Zielwiegen", "250 Gramm", "Ziel setzen: 50, 100, 150, 200, 250, 300, 500, 1000" },
  { NULL, "Stopp", "Ziel beenden (auch „Abbrechen“)" },
  { "Bedienen", "Weiter", "nächster Schritt, z. B. im Rezept" },
  { NULL, "Zurück", "zur Startseite" },
  { NULL, "Nochmal", "letzte Ansage wiederholen" },
  { "Sonstiges", "Lauter", "oder „Leiser“" },
  { NULL, "Akku", "Ladestand ansagen" },
  { NULL, "Uhrzeit", "wie spät es ist" },
  { NULL, "Hilfe", "diese Liste" },
};

static void quoted(char *b, int len, const char *word) {
  snprintf(b, len, T("„%s“"), T(word));
}

// Liste in einen Behälter (Setup-Seite und Hilfe-Fenster)
static lv_obj_t *build_list(lv_obj_t *parent, int h) {
  lv_obj_t *col = ui_box(parent);
  lv_obj_set_size(col, 280, h);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(col, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(col, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_row(col, 2, 0);
  lv_obj_set_style_pad_bottom(col, 24, 0);  // letzter Eintrag rollt ganz über den Knopf hinaus

  lv_obj_t *how = ui_label(col, "Sag „Hallo Waage“, warte auf „Ja?“, dann den Befehl. Oder oben zweimal tippen.",
                           &font_sg_14, C_MUTED);
  lv_obj_set_width(how, 280);
  lv_label_set_long_mode(how, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(how, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_bottom(how, 6, 0);

  char b[64];
  for (unsigned i = 0; i < sizeof(CMDS) / sizeof(CMDS[0]); i++) {
    if (CMDS[i].group) {
      lv_obj_t *g = ui_label(col, CMDS[i].group, &font_sg_14, C_ACCENT);
      lv_obj_set_width(g, 280);
      lv_obj_set_style_text_align(g, LV_TEXT_ALIGN_LEFT, 0);
      lv_obj_set_style_pad_top(g, i ? 14 : 0, 0);
    }
    lv_obj_t *row = ui_box(col);
    lv_obj_set_size(row, 280, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(row, 4, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    quoted(b, sizeof(b), CMDS[i].say);
    lv_obj_t *s = ui_label(row, b, &font_sg_18, C_TEXT);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_t *d = ui_label(row, CMDS[i].does, &font_sg_14, C_MUTED);
    lv_obj_set_width(d, 280);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_LEFT, 0);
  }
  return col;
}

// ------------------------------------------------------------
//  Setup -> Ton -> Befehle
// ------------------------------------------------------------
static void pc_back_cb(lv_event_t *e) { ui_switch_page(page_setup_cat_create(2)); }

lv_obj_t *page_voice_cmds_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Sprachbefehle", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_t *l = build_list(s, 236);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, 2);
  lv_obj_t *b = ui_btn(s, "Zurück", BTN_NORMAL);
  lv_obj_set_height(b, 46);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 146);
  lv_obj_add_event_cb(b, pc_back_cb, LV_EVENT_CLICKED, NULL);
  ui_set_back([]() { return page_setup_cat_create(2); });
  return s;
}

// ------------------------------------------------------------
//  Fenster über der aktuellen Seite (Hinweis, Bestätigung, Hilfe)
// ------------------------------------------------------------
static lv_obj_t *s_hint = NULL;   // „Ich höre zu …“ bzw. Bestätigung
static lv_obj_t *s_help = NULL;   // Liste aller Befehle
static lv_timer_t *s_help_tm = NULL;
static bool s_blocked = false;
static bool s_was_awake = false;
static uint32_t s_wake_ms = 0, s_confirm_until = 0;
static unsigned long s_last_seen = 0;

void voice_help_block(bool on) { s_blocked = on; }

static lv_obj_t *layer(bool clickable) {
  lv_obj_t *o = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(o, C_BG, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);  // deckend: darunter durchscheinender Text stört beim Lesen
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  if (clickable) lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);  // nichts darunter antippen
  else lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

static void hint_close() {
  if (s_hint) lv_obj_del(s_hint);
  s_hint = NULL;
}

static void help_close() {
  if (s_help_tm) lv_timer_del(s_help_tm);
  s_help_tm = NULL;
  if (s_help) lv_obj_del(s_help);
  s_help = NULL;
}

static void help_close_cb(lv_event_t *e) { help_close(); }
static void help_timer_cb(lv_timer_t *t) {
  s_help_tm = NULL;  // löscht sich selbst (einmalig)
  help_close();
}

void voice_help_show_all() {
  hint_close();
  help_close();
  s_help = layer(true);
  lv_obj_t *t = ui_label(s_help, "Sprachbefehle", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_t *l = build_list(s_help, 236);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, 2);
  lv_obj_t *b = ui_btn(s_help, "Schließen", BTN_NORMAL);
  lv_obj_set_height(b, 46);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 146);
  lv_obj_add_event_cb(b, help_close_cb, LV_EVENT_CLICKED, NULL);
  s_help_tm = lv_timer_create(help_timer_cb, HELP_OPEN_MS, NULL);
  lv_timer_set_repeat_count(s_help_tm, 1);
}

// Drei Beispiele, passend zu dem, was gerade offen ist
static void examples(const char *ex[3]) {
  if (ui_target_goal() > 0 || page_ziel_running() > 0) {
    ex[0] = "Wie viel?";  // sagt dann die Restmenge
    ex[1] = "Stopp";
    ex[2] = "Tara";
  } else if (ui_has_button(VOICE_NEXT_LABELS, VOICE_NEXT_COUNT)) {
    ex[0] = "Weiter";
    ex[1] = "Wie viel?";
    ex[2] = "Zurück";
  } else {
    ex[0] = "Tara";
    ex[1] = "Wie viel?";
    ex[2] = "250 Gramm";
  }
}

static void hint_show() {
  hint_close();
  s_hint = layer(false);
  lv_obj_t *ic = ui_label(s_hint, ICON_MIKRO, &font_icons_36, C_ACCENT);
  lv_obj_align(ic, LV_ALIGN_CENTER, 0, -118);
  lv_obj_t *t = ui_label(s_hint, "Ich höre zu …", &font_sg_24, C_ACCENT);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -76);
  lv_obj_t *z = ui_label(s_hint, "Sag zum Beispiel:", &font_sg_14, C_MUTED);
  lv_obj_align(z, LV_ALIGN_CENTER, 0, -36);
  const char *ex[3];
  examples(ex);
  char b[48];
  for (int i = 0; i < 3; i++) {
    quoted(b, sizeof(b), ex[i]);
    lv_obj_t *e = ui_label(s_hint, b, &font_sg_24, C_TEXT);
    lv_obj_align(e, LV_ALIGN_CENTER, 0, -4 + i * 38);
  }
  lv_obj_t *h = ui_label(s_hint, "„Hilfe“ zeigt alle Befehle", &font_sg_14, C_MUTED);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 120);
}

// Text zum verstandenen Befehl ("ziel_250" -> „250 Gramm“)
static const char *word_text(const char *id) {
  for (int i = 0; i < voice_words(); i++)
    if (!strcmp(voice_word_id(i), id)) return voice_word_text(i);
  return NULL;
}

static void confirm_show(const char *id) {
  const char *w = word_text(id);
  if (!w || id[0] == '_' || !strcmp(id, "hallo_waage")) {
    hint_close();
    return;
  }
  hint_close();
  s_hint = layer(false);
  lv_obj_t *ic = ui_label(s_hint, ICON36_HAKEN, &font_icons_36, C_ACCENT);
  lv_obj_align(ic, LV_ALIGN_CENTER, 0, -40);
  char b[48];
  quoted(b, sizeof(b), w);
  lv_obj_t *t = ui_label(s_hint, b, &font_sg_34, C_TEXT);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, 12);
  s_confirm_until = hal_millis() + CONFIRM_MS;
}

void voice_help_tick() {
  bool aw = voice_awake() && !s_blocked && !s_help;
  uint32_t now = hal_millis();
  if (aw && !s_was_awake) {  // gerade geweckt
    s_wake_ms = now;
    s_last_seen = voice_last_ms();
    s_confirm_until = 0;
    hint_show();
  } else if (!aw && s_was_awake) {  // Zuhören vorbei: Befehl verstanden oder Zeit um
    bool cmd = voice_last_ms() != s_last_seen && voice_last()[0];
    if (cmd && !s_blocked && !s_help) confirm_show(voice_last());
    else hint_close();
  }
  if (s_confirm_until && (int32_t)(now - s_confirm_until) >= 0) {
    s_confirm_until = 0;
    hint_close();
  }
  s_was_awake = aw;
}
