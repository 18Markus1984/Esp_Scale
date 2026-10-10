// ============================================================
//  Modi -> Rezept (siehe Design-Sheet)
//   Auswahl:  Rezepte aus /Waage/Rezepte/*.txt als Kurvenliste
//   Vorschau: Zutaten, Portionen skalieren
//   Schritt:  Ring füllt sich zum Ziel, vor jeder Zutat Auto-Tara
//   Fertig:   Zusammenfassung, Eintrag im Protokoll
//
//  Dateiformat (Beispiel pfannkuchen.txt):
//    name=Pfannkuchen
//    portionen=2
//    Mehl;250
//    Milch;500
//
//  Anweisungen („>ruehren;Glatt rühren;2“) zeigen statt der Gewichtsanzeige
//  ein großes Icon und den Text; mit Dauer startet ein Knopf einen Küchentimer.
//
//  Rezepte mit Eiern (Zutat „Ei“/„Eier“) skalieren nicht nach Portionen,
//  sondern nach ganzen Eiern: Die Anzahl im Grundrezept ist Eigewicht / EGG_G.
//  Die Eier werden zuerst abgewogen, danach richten sich alle anderen
//  Zutaten nach dem tatsächlichen Eigewicht (Größe S, M oder L egal).
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "scale.h"
#include "storage.h"
#include "hal.h"
#include "data.h"
#include "sound.h"
#include "settings.h"
#include "ui_text.h"
#include "config.h"
#include "tools.h"
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <math.h>

#define LIST_MAX 20
#define TOL_G 2.0f

static recipe_t s_rec;
static int s_portions = 2;
static int s_step = 0;
static float s_done_g[RECIPE_MAX_ING];  // tatsächlich abgewogen je Zutat
static uint32_t s_start_ms = 0;

static char l_files[LIST_MAX][48];
static char l_names[LIST_MAX][40];
static int l_counts[LIST_MAX];

static lv_obj_t *page_preview_create();
static lv_obj_t *page_edit_create();
static recipe_t s_draft;  // neues Rezept, bis "Speichern"
static lv_obj_t *page_step_create();
static lv_obj_t *page_done_create();

// ---- Eier ----
static int s_egg = -1;        // Index der Zutat „Eier“, -1 = keine
static int s_egg_base = 1;    // Eier im Grundrezept
static int s_eggs = 1;        // gewählte Anzahl
static float s_factor = 1.0f; // Faktor für alle Zutaten (nach dem Abwiegen: echtes Eigewicht)
static bool s_factor_real = false;  // Faktor kommt aus dem gewogenen Eigewicht
static int s_order[RECIPE_MAX_ING]; // Reihenfolge beim Abwiegen: Eier zuerst

static bool is_egg(const char *name) {
  char n[16];
  int k = 0;
  while (*name == ' ') name++;
  while (*name && k < (int)sizeof(n) - 1) n[k++] = *name++;
  while (k > 0 && n[k - 1] == ' ') k--;
  n[k] = 0;
  return !strcasecmp(n, "Ei") || !strcasecmp(n, "Eier") || !strcasecmp(n, "Egg") || !strcasecmp(n, "Eggs");
}

// Eier im Rezept suchen und die Abwiege-Reihenfolge festlegen
static void egg_setup() {
  s_egg = -1;
  for (int i = 0; i < s_rec.count && s_egg < 0; i++)
    if (s_rec.ing[i].kind == STEP_WEIGH && is_egg(s_rec.ing[i].name) && s_rec.ing[i].grams > 0) s_egg = i;
  int k = 0;
  if (s_egg >= 0) s_order[k++] = s_egg;
  for (int i = 0; i < s_rec.count; i++)
    if (i != s_egg) s_order[k++] = i;
  if (s_egg >= 0) {
    s_egg_base = (int)lroundf(s_rec.ing[s_egg].grams / EGG_G);
    if (s_egg_base < 1) s_egg_base = 1;
    s_eggs = s_egg_base;
  }
  s_factor = 1.0f;
  s_factor_real = false;
}

// geplanter Faktor aus der gewählten Anzahl Eier bzw. Portionen
static float plan_factor() {
  if (s_egg >= 0) return (float)s_eggs / s_egg_base;
  return (float)s_portions / s_rec.portions;
}

static float target(int i) {
  return s_rec.ing[i].grams * s_factor;
}

static int step_ing() {  // Zutat des aktuellen Schritts
  return s_order[s_step];
}

static bool step_is_note(int step) {
  return step >= 0 && step < s_rec.count && s_rec.ing[s_order[step]].kind == STEP_NOTE;
}

// Icons der Anweisungen (Material Symbols in font_icons_80, Reihenfolge wie SI_* in data.h)
static const char *const SI_GLYPH[SI_COUNT] = {
  "\xEE\x9F\x93",  // soup_kitchen   Rühren
  "\xEE\xBF\xA3",  // blender        Mixen
  "\xEE\x9D\xA4",  // back_hand      Kneten
  "\xEF\x95\x83",  // skillet        Braten
  "\xEF\x95\x85",  // stockpot       Kochen
  "\xEE\xA1\x83",  // oven_gen       Backen
  "\xEE\xA9\x87",  // outdoor_grill  Grillen
  "\xEE\x85\x8E",  // content_cut    Schneiden
  "\xEE\xAC\xBB",  // ac_unit        Kühlen
  "\xEE\xA9\x9B",  // hourglass_top  Ruhen
  "\xEF\x88\x84",  // microwave      Mikrowelle
  "\xEE\x9E\x98",  // water_drop     Gießen
  "\xEE\xBD\x95",  // local_fire_department  Erhitzen
  "\xEE\x95\xAC",  // restaurant     Servieren
  "\xEE\xA2\x8E",  // info           Hinweis
};

static int s_step_timer[RECIPE_MAX_ING];  // Küchentimer je Schritt, -1 = keiner

// ------------------------------------------------------------
//  Auswahl
// ------------------------------------------------------------
static void pick_cb(int index) {
  if (index == 0) {  // "+ Neues Rezept"
    memset(&s_draft, 0, sizeof(s_draft));
    s_draft.portions = 2;
    ui_switch_page(page_edit_create());
    return;
  }
  if (!recipe_load(l_files[index - 1], &s_rec)) return;
  s_portions = s_rec.portions;
  egg_setup();
  ui_switch_page(page_preview_create());
}

lv_obj_t *page_rezept_create() {
  lv_obj_t *s = ui_screen_create();
  if (!storage_ok()) {
    lv_obj_t *l = ui_label(s, "Keine SD-Karte", &font_sg_18, C_MUTED);
    lv_obj_center(l);
    return s;
  }
  int n = recipes_list(l_files, l_names, l_counts, LIST_MAX);

  static char subs[LIST_MAX][16];
  static ui_list_item_t items[LIST_MAX + 1];
  items[0].title = "+ Neues Rezept";
  items[0].sub = "anlegen";
  for (int i = 0; i < n; i++) {
    snprintf(subs[i], sizeof(subs[i]), T("%d Zutaten"), l_counts[i]);
    items[i + 1].title = l_names[i];
    items[i + 1].sub = subs[i];
  }
  lv_obj_t *list = ui_curved_list(s, items, n + 1, pick_cb);
  // gleich das erste echte Rezept markieren, falls vorhanden
  if (n > 0) lv_obj_scroll_to_view(lv_obj_get_child(list, 1), LV_ANIM_OFF);

  lv_obj_t *t = ui_label(s, "Rezepte · SD-Karte", &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return s;
}

// ------------------------------------------------------------
//  Vorschau
// ------------------------------------------------------------
static lv_obj_t *pv_rows[3][2], *pv_more, *pv_port;
static int pv_ing[3];  // Zutat je Vorschauzeile (nur Zutaten, keine Anweisungen)
static int pv_n;

static void pv_refresh() {
  char b[32];
  s_factor = plan_factor();
  s_factor_real = false;
  for (int r = 0; r < pv_n; r++) {
    int i = pv_ing[r];
    // Eier: ungefähres Gewicht, die echten Eier wiegen mehr oder weniger
    snprintf(b, sizeof(b), i == s_egg ? T("ca. %d g") : "%d g", (int)lroundf(target(i)));
    lv_label_set_text(pv_rows[r][1], b);
  }
  if (s_egg >= 0) {
    if (s_eggs == 1) snprintf(b, sizeof(b), "%s", T("1 Ei"));
    else snprintf(b, sizeof(b), T("%d Eier"), s_eggs);
  } else snprintf(b, sizeof(b), T("%d Port."), s_portions);
  lv_label_set_text(pv_port, b);
}

// Minus/Plus: ganze Eier oder Portionen
static void pv_minus_cb(lv_event_t *e) {
  int *n = s_egg >= 0 ? &s_eggs : &s_portions;
  if (*n > 1) (*n)--;
  pv_refresh();
}

static void pv_plus_cb(lv_event_t *e) {
  int *n = s_egg >= 0 ? &s_eggs : &s_portions;
  if (*n < 20) (*n)++;
  pv_refresh();
}

static lv_obj_t *pv_del;
static bool pv_confirm = false;

static void pv_delete_cb(lv_event_t *e) {
  if (!pv_confirm) {  // erst beim zweiten Tippen löschen
    pv_confirm = true;
    lv_label_set_text(lv_obj_get_child(pv_del, 0), "Wirklich?");
    lv_obj_set_style_bg_color(pv_del, C_DANGER, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(pv_del, 0), C_ON_ACCENT, 0);
    return;
  }
  recipe_delete(s_rec.file);
  ui_switch_page(page_rezept_create());
}

static void pv_start_cb(lv_event_t *e) {
  s_step = 0;
  s_start_ms = hal_millis();
  memset(s_done_g, 0, sizeof(s_done_g));
  for (int i = 0; i < RECIPE_MAX_ING; i++) s_step_timer[i] = -1;
  s_factor = plan_factor();
  s_factor_real = false;
  scale_tare();
  ui_switch_page(page_step_create());
}

static lv_obj_t *page_preview_create() {
  lv_obj_t *s = ui_screen_create();
  char b[64];

  snprintf(b, sizeof(b), T("%s · %d Zutaten"), s_rec.name, s_rec.weigh);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 56);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 270, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -52);
  pv_n = 0;
  for (int k = 0; k < s_rec.count && pv_n < 3; k++)
    if (s_rec.ing[s_order[k]].kind == STEP_WEIGH) pv_ing[pv_n++] = s_order[k];
  for (int i = 0; i < pv_n; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 270, 34);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    pv_rows[i][0] = ui_label(row, s_rec.ing[pv_ing[i]].name, &font_sg_18, C_TEXT);
    lv_obj_align(pv_rows[i][0], LV_ALIGN_LEFT_MID, 0, 0);
    pv_rows[i][1] = ui_label(row, "", &font_sg_18, C_MUTED);
    lv_obj_align(pv_rows[i][1], LV_ALIGN_RIGHT_MID, 0, 0);
  }
  int notes = s_rec.count - s_rec.weigh;
  if (s_rec.weigh > 3 || notes > 0) {
    b[0] = 0;
    if (s_rec.weigh > 3) snprintf(b, sizeof(b), T("+ %d weitere"), s_rec.weigh - 3);
    if (notes > 0) {
      char n[32];
      if (notes == 1) snprintf(n, sizeof(n), "%s", T("1 Anweisung"));
      else snprintf(n, sizeof(n), T("%d Anweisungen"), notes);
      if (b[0]) strncat(b, " · ", sizeof(b) - strlen(b) - 1);
      strncat(b, n, sizeof(b) - strlen(b) - 1);
    }
    pv_more = ui_label(list, b, &font_sg_14, C_MUTED);
  }

  pv_port = ui_label(s, "", &font_sg_24, C_TEXT);
  lv_obj_align(pv_port, LV_ALIGN_CENTER, 0, 46);
  lv_obj_t *m = ui_round_btn(s, "−", pv_minus_cb);
  lv_obj_align(m, LV_ALIGN_CENTER, -100, 46);
  lv_obj_t *p = ui_round_btn(s, "+", pv_plus_cb);
  lv_obj_align(p, LV_ALIGN_CENTER, 100, 46);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 122);
  pv_del = ui_btn(row, "Löschen", BTN_NORMAL);
  lv_obj_set_style_text_color(lv_obj_get_child(pv_del, 0), C_DANGER, 0);
  lv_obj_add_event_cb(pv_del, pv_delete_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *go = ui_btn(row, "Starten", BTN_PRIMARY);
  lv_obj_add_event_cb(go, pv_start_cb, LV_EVENT_CLICKED, NULL);
  pv_confirm = false;

  pv_refresh();
  return s;
}

// ------------------------------------------------------------
//  Schritt
// ------------------------------------------------------------
static lv_obj_t *st_ring, *st_weight, *st_goal;
static lv_timer_t *st_timer;
static int st_prev;

static void st_next_cb(lv_event_t *e);

static void st_next_cb(lv_event_t *e) {
  s_done_g[s_step] = step_is_note(s_step) ? 0 : scale_net();
  // Eier gewogen: alle weiteren Zutaten nach dem echten Eigewicht ausrichten
  // (unter 10 g wurde wohl nichts aufgelegt, dann bleibt es beim Plan)
  if (step_ing() == s_egg && s_done_g[s_step] > 10.0f) {
    s_factor = s_done_g[s_step] / s_rec.ing[s_egg].grams;
    s_factor_real = true;
  }
  if (s_step + 1 >= s_rec.count) {
    ui_switch_page(page_done_create());
    return;
  }
  s_step++;
  if (!step_is_note(s_step)) scale_tare();  // Auto-Tara vor der nächsten Zutat
  ui_switch_page(page_step_create());
}

static void st_back_cb(lv_event_t *e) {
  if (s_step == 0 || (s_step == 1 && s_egg >= 0)) {  // zurück zu den Eiern: wieder nach Plan
    s_factor = plan_factor();
    s_factor_real = false;
  }
  if (s_step == 0) {
    ui_switch_page(page_preview_create());
    return;
  }
  s_step--;
  if (!step_is_note(s_step)) scale_tare();
  ui_switch_page(page_step_create());
}

// Ist die Zutat fertig eingewogen und liegt ruhig, automatisch weiterschalten
static uint32_t st_ok_since = 0;

static void st_auto(int state) {
  if (!g_set.auto_next || state == 0) {
    st_ok_since = 0;
    return;
  }
  if (!scale_stable()) {
    st_ok_since = 0;
    return;
  }
  uint32_t now = hal_millis();
  if (st_ok_since == 0) st_ok_since = now;
  if (now - st_ok_since < 1200) return;
  st_ok_since = 0;
  st_next_cb(NULL);  // tariert und geht zum nächsten Schritt
}

static void st_tara_cb(lv_event_t *e) {
  scale_tare();
  sound_play(SND_TARA);
}

static void st_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  float goal = target(step_ing());
  bool egg = step_ing() == s_egg;
  int v = (int)(g / goal * 1000.0f);
  if (v < 0) v = 0;
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(st_ring) != v) lv_arc_set_value(st_ring, v);

  char w[16];
  snprintf(w, sizeof(w), "%d", (int)lroundf(g));
  ui_label_update(st_weight, w);

  // Toleranz: max. 2 g, bei kleinen Mengen 5 % (mind. 0,5 g)
  float tol = fmaxf(0.5f, fminf(TOL_G, goal * 0.05f));
  int state = g > goal + tol ? 2 : ((g >= goal - tol && g > 0.3f) ? 1 : 0);
  if (egg) {
    // Eier sind nie „zu viel“: Der Rest des Rezepts passt sich an.
    // Kein Parkpiepser, kein Auto-Weiter (zwischen zwei Eiern liegt es ja auch ruhig).
    state = g >= goal * 0.8f ? 1 : 0;
  } else {
    sound_parking(g, goal, tol);  // Parkpiepser
  }
  if (state != st_prev) {
    st_prev = state;
    lv_color_t c = state == 2 ? C_DANGER : C_ACCENT;
    lv_obj_set_style_arc_color(st_ring, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(st_weight, state == 2 ? C_DANGER : (state == 1 ? C_ACCENT : C_TEXT), 0);
  }
  st_auto(egg ? 0 : state);
}

// ------------------------------------------------------------
//  Anweisung: Icon und Text statt Gewicht, optional Küchentimer
// ------------------------------------------------------------
static lv_obj_t *nt_ring, *nt_btn, *nt_hint;

static void nt_timer_cb(lv_timer_t *t) {
  int ti = s_step_timer[s_step];
  ingredient_t *z = &s_rec.ing[step_ing()];
  if (!nt_btn || !z->secs) return;
  char b[24];
  if (ti >= 0 && timer_running(ti)) {
    int left = timer_left(ti);
    timer_fmt(b, sizeof(b), left < 0 ? 0 : left);
    uint32_t dur = timer_duration(ti);
    int v = dur ? (int)(1000L * (dur - (left < 0 ? 0 : left)) / dur) : 0;
    if (lv_arc_get_value(nt_ring) != v) lv_arc_set_value(nt_ring, v);
  } else {
    char t[16];
    timer_fmt(t, sizeof(t), z->secs);
    snprintf(b, sizeof(b), T("Timer %s"), t);
    if (ti >= 0) {  // abgelaufen
      s_step_timer[s_step] = -1;
      lv_arc_set_value(nt_ring, 1000);
    }
  }
  ui_label_update(lv_obj_get_child(nt_btn, 0), b);
}

static void nt_timer_start_cb(lv_event_t *e) {
  ingredient_t *z = &s_rec.ing[step_ing()];
  int ti = s_step_timer[s_step];
  if (ti >= 0 && timer_running(ti)) return;  // läuft schon
  ti = timer_free();
  if (ti < 0) {
    ui_label_update(nt_hint, T("Kein Timer frei"));
    sound_play(SND_WARN);
    return;
  }
  timer_start(ti, z->secs, T(step_icon_label(z->icon)));
  s_step_timer[s_step] = ti;
  sound_play(SND_CLICK);
  lv_arc_set_value(nt_ring, 0);
  nt_timer_cb(NULL);
}

static lv_obj_t *page_note_create() {
  lv_obj_t *s = ui_screen_create();
  ingredient_t *z = &s_rec.ing[step_ing()];
  char b[48];
  nt_ring = ui_ring(s, 396);
  lv_arc_set_value(nt_ring, 0);

  snprintf(b, sizeof(b), T("Schritt %d / %d"), s_step + 1, s_rec.count);
  lv_obj_t *l = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 50);

  // Icon statt Gewicht, in der Akzentfarbe des Farbschemas
  lv_obj_t *ic = ui_label(s, SI_GLYPH[z->icon < SI_COUNT ? z->icon : SI_HINWEIS], &font_icons_80, C_ACCENT);
  lv_obj_align(ic, LV_ALIGN_CENTER, 0, -62);

  // Text: kurze Anweisungen groß, längere kleiner und umgebrochen
  const char *txt = z->text[0] ? z->text : T(step_icon_label(z->icon));
  bool big = strlen(txt) <= 30;
  lv_obj_t *t = ui_label(s, txt, big ? &font_sg_24 : &font_sg_18, C_TEXT);
  lv_obj_set_width(t, 300);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, 30);

  nt_hint = ui_label(s, "", &font_sg_14, C_WARN);
  lv_obj_align(nt_hint, LV_ALIGN_CENTER, 0, 150);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 108);
  lv_obj_t *back = ui_btn(row, "Zurück", BTN_NORMAL);
  lv_obj_set_style_pad_hor(back, 14, 0);
  lv_obj_add_event_cb(back, st_back_cb, LV_EVENT_CLICKED, NULL);
  nt_btn = NULL;
  if (z->secs) {
    nt_btn = ui_btn(row, "", BTN_NORMAL);
    lv_obj_set_style_pad_hor(nt_btn, 14, 0);
    lv_obj_add_event_cb(nt_btn, nt_timer_start_cb, LV_EVENT_CLICKED, NULL);
  }
  lv_obj_t *next = ui_btn(row, s_step + 1 >= s_rec.count ? "Fertig" : "Weiter", BTN_PRIMARY);
  lv_obj_add_event_cb(next, st_next_cb, LV_EVENT_CLICKED, NULL);

  sound_parking_reset();
  if (nt_btn) {
    ui_page_timer(s, nt_timer_cb, 250);
    nt_timer_cb(NULL);
  }
  return s;
}

static lv_obj_t *page_step_create() {
  if (step_is_note(s_step)) return page_note_create();
  lv_obj_t *s = ui_screen_create();
  char b[48];
  st_ring = ui_ring(s, 396);

  snprintf(b, sizeof(b), T("Schritt %d / %d"), s_step + 1, s_rec.count);
  lv_obj_t *l = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 50);

  int ing = step_ing();
  lv_obj_t *name = ui_label(s, s_rec.ing[ing].name, &font_sg_24, C_TEXT);
  lv_obj_align(name, LV_ALIGN_CENTER, 0, -104);

  st_weight = ui_label(s, "0", &font_sg_80, C_TEXT);
  lv_obj_align(st_weight, LV_ALIGN_CENTER, 0, -30);

  if (ing == s_egg) {
    if (s_eggs == 1) snprintf(b, sizeof(b), "%s", T("1 Ei aufschlagen"));
    else snprintf(b, sizeof(b), T("%d Eier aufschlagen"), s_eggs);
  } else snprintf(b, sizeof(b), T("von %d g"), (int)lroundf(target(ing)));
  st_goal = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(st_goal, LV_ALIGN_CENTER, 0, 30);
  // Hinweis, dass sich die Menge nach den gewogenen Eiern richtet
  if (s_factor_real && ing != s_egg) {
    snprintf(b, sizeof(b), T("an %d g Eier angepasst"), (int)lroundf(s_factor * s_rec.ing[s_egg].grams));
    lv_obj_t *h = ui_label(s, b, &font_sg_14, C_FAINT);
    lv_obj_align(h, LV_ALIGN_CENTER, 0, 56);
  }

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 100);
  lv_obj_t *tara = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_set_style_pad_hor(tara, 14, 0);
  lv_obj_add_event_cb(tara, st_tara_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *back = ui_btn(row, "Zurück", BTN_NORMAL);
  lv_obj_set_style_pad_hor(back, 14, 0);
  lv_obj_add_event_cb(back, st_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *next = ui_btn(row, s_step + 1 >= s_rec.count ? "Fertig" : "Weiter", BTN_PRIMARY);
  lv_obj_add_event_cb(next, st_next_cb, LV_EVENT_CLICKED, NULL);

  st_prev = -1;
  sound_parking_reset();
  st_timer = ui_page_timer(s, st_timer_cb, 100);
  st_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Fertig
// ------------------------------------------------------------
static void dn_again_cb(lv_event_t *e) {
  ui_switch_page(page_preview_create());
}

static void dn_home_cb(lv_event_t *e) {
  ui_go_home();
}

static lv_obj_t *page_done_create() {
  // Protokoll: Gesamtgewicht mit Rezeptname
  float sum = 0;
  for (int i = 0; i < s_rec.count; i++) sum += s_done_g[i];
  char note[48];
  snprintf(note, sizeof(note), T("Rezept: %s"), s_rec.name);
  log_add(sum, note);
  sound_play(SND_DONE);

  lv_obj_t *s = ui_screen_create();
  lv_obj_t *ring = ui_ring(s, 396);
  lv_arc_set_value(ring, 1000);

  lv_obj_t *h = ui_label(s, "Fertig", &font_sg_24, C_ACCENT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -80);
  lv_obj_t *n = ui_label(s, s_rec.name, &font_sg_34, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -36);

  char b[48];
  int min = (int)((hal_millis() - s_start_ms) / 60000);
  snprintf(b, sizeof(b), T("%d von %d Zutaten · %d min"), s_rec.weigh, s_rec.weigh, min);
  lv_obj_t *i = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(i, LV_ALIGN_CENTER, 0, 6);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 80);
  lv_obj_t *a = ui_btn(row, "Nochmal", BTN_NORMAL);
  lv_obj_add_event_cb(a, dn_again_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *z = ui_btn(row, "Zu Wiegen", BTN_PRIMARY);
  lv_obj_add_event_cb(z, dn_home_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

// ============================================================
//  Neues Rezept anlegen
//   Name (Ring) -> Zutatenliste -> Zutat: Name (Ring) + Menge
//   (einstellen oder direkt abwiegen) -> Speichern auf SD
// ============================================================
static const char *const INGREDIENTS[] = {
  "Mehl", "Weizenmehl", "Zucker", "Puderzucker", "Vanillezucker", "Butter", "Margarine", "Milch",
  "Wasser", "Eier", "Salz", "Hefe", "Backpulver", "Öl", "Olivenöl", "Sahne", "Quark", "Joghurt",
  "Reis", "Nudeln", "Haferflocken", "Kakao", "Schokolade", "Honig", "Nüsse", "Mandeln", "Rosinen",
  "Kartoffeln", "Zwiebeln", "Tomaten", "Käse", "Hackfleisch",
};
static const int INGREDIENTS_N = sizeof(INGREDIENTS) / sizeof(INGREDIENTS[0]);

static char a_name[32];
static int a_grams = 100;
static int a_step = 10;

static lv_obj_t *page_amount_create();

// ---------- Name des Rezepts ----------
static void rname_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(s_draft.name, text, sizeof(s_draft.name) - 1);
    s_draft.name[sizeof(s_draft.name) - 1] = 0;
  }
  ui_switch_page(page_edit_create());
}

static void ed_name_cb(lv_event_t *e) {
  ui_switch_page(ui_text_page_create("Neues Rezept · Name", s_draft.name, 39, false, NULL, 0, rname_done));
}

// ---------- Zutatenliste ----------
static lv_obj_t *ed_port, *ed_toast;

static void ed_port_show() {
  char b[24];
  snprintf(b, sizeof(b), T("für %d Port."), s_draft.portions);
  lv_label_set_text(ed_port, b);
}

static void ed_port_cb(lv_event_t *e) {
  int up = (int)(intptr_t)lv_event_get_user_data(e);
  s_draft.portions += up ? 1 : -1;
  if (s_draft.portions < 1) s_draft.portions = 1;
  if (s_draft.portions > 20) s_draft.portions = 20;
  ed_port_show();
}

static void ing_name_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(a_name, text, sizeof(a_name) - 1);
    a_name[sizeof(a_name) - 1] = 0;
    ui_switch_page(page_amount_create());
  } else {
    ui_switch_page(page_edit_create());
  }
}

static void ed_add_cb(lv_event_t *e) {
  if (s_draft.count >= RECIPE_MAX_ING) return;
  ui_switch_page(ui_text_page_create("Zutat · Name", "", 31, false, INGREDIENTS, INGREDIENTS_N, ing_name_done));
}

static void ed_remove_cb(lv_event_t *e) {  // gedrückt halten = Zutat entfernen
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  for (int k = i; k < s_draft.count - 1; k++) s_draft.ing[k] = s_draft.ing[k + 1];
  s_draft.count--;
  s_draft.weigh--;
  ui_switch_page(page_edit_create());
}

static void ed_save_cb(lv_event_t *e) {
  if (!s_draft.name[0]) {
    ui_toast_show(ed_toast, "Bitte zuerst einen Namen", C_WARN);
    return;
  }
  if (s_draft.count == 0) {
    ui_toast_show(ed_toast, "Noch keine Zutaten", C_WARN);
    return;
  }
  if (!recipe_save(&s_draft, NULL, 0)) {
    sound_play(SND_WARN);
    ui_toast_show(ed_toast, "Speichern fehlgeschlagen", C_WARN);
    return;
  }
  sound_play(SND_SAVE);
  ui_switch_page(page_rezept_create());
}

static lv_obj_t *page_edit_create() {
  lv_obj_t *s = ui_screen_create();
  char b[64];

  snprintf(b, sizeof(b), "%s ›", s_draft.name[0] ? s_draft.name : T("Name eingeben"));
  lv_obj_t *nb = ui_btn(s, b, BTN_NORMAL);
  lv_obj_set_height(nb, 44);
  lv_obj_align(nb, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_add_event_cb(nb, ed_name_cb, LV_EVENT_CLICKED, NULL);

  ed_port = ui_label(s, "", &font_sg_18, C_TEXT);
  lv_obj_align(ed_port, LV_ALIGN_CENTER, 0, -92);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_set_size(m, 46, 46);
  lv_obj_align(m, LV_ALIGN_CENTER, -100, -92);
  lv_obj_add_event_cb(m, ed_port_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_set_size(p, 46, 46);
  lv_obj_align(p, LV_ALIGN_CENTER, 100, -92);
  lv_obj_add_event_cb(p, ed_port_cb, LV_EVENT_CLICKED, (void *)1);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 280, 120);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -8);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  if (s_draft.count == 0) ui_label(list, "Noch keine Zutaten", &font_sg_18, C_MUTED);
  for (int i = 0; i < s_draft.count; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 280, 36);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_add_event_cb(row, ed_remove_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    lv_obj_t *n = ui_label(row, s_draft.ing[i].name, &font_sg_18, C_TEXT);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
    snprintf(b, sizeof(b), "%d g", (int)lroundf(s_draft.ing[i].grams));
    lv_obj_t *g = ui_label(row, b, &font_sg_18, C_MUTED);
    lv_obj_align(g, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  lv_obj_t *hint = ui_label(s, "Halten: Zutat entfernen", &font_sg_14, C_FAINT);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 64);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 110);
  lv_obj_t *add = ui_btn(row, "+ Zutat", BTN_NORMAL);
  lv_obj_add_event_cb(add, ed_add_cb, LV_EVENT_CLICKED, NULL);
  if (s_draft.count >= RECIPE_MAX_ING) lv_obj_add_state(add, LV_STATE_DISABLED);
  lv_obj_t *sv = ui_btn(row, "Speichern", BTN_PRIMARY);
  lv_obj_add_event_cb(sv, ed_save_cb, LV_EVENT_CLICKED, NULL);

  ed_toast = ui_toast_create(s);
  lv_obj_align(ed_toast, LV_ALIGN_CENTER, 0, 156);

  ed_port_show();
  return s;
}

// ---------- Menge einer Zutat ----------
static lv_obj_t *am_value, *am_live, *am_steps[3];
static const int AM_STEPS[3] = { 1, 10, 100 };

static void am_show() {
  char b[16];
  snprintf(b, sizeof(b), "%d g", a_grams);
  lv_label_set_text(am_value, b);
  for (int i = 0; i < 3; i++) {
    bool on = AM_STEPS[i] == a_step;
    lv_obj_set_style_bg_color(am_steps[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(am_steps[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

static void am_adj_cb(lv_event_t *e) {
  int up = (int)(intptr_t)lv_event_get_user_data(e);
  a_grams += up ? a_step : -a_step;
  if (a_grams < 1) a_grams = 1;
  if (a_grams > 9999) a_grams = 9999;
  am_show();
}

static void am_step_cb(lv_event_t *e) {
  a_step = (int)(intptr_t)lv_event_get_user_data(e);
  am_show();
}

static void am_weigh_cb(lv_event_t *e) {  // aufgelegte Menge übernehmen
  float g = scale_net();
  if (g >= 0.5f) {
    a_grams = (int)lroundf(g);
    sound_play(SND_TARA);
    am_show();
  }
}

static void am_ok_cb(lv_event_t *e) {
  ingredient_t *i = &s_draft.ing[s_draft.count++];
  memset(i, 0, sizeof(*i));
  i->kind = STEP_WEIGH;
  s_draft.weigh++;
  strncpy(i->name, a_name, sizeof(i->name) - 1);
  i->name[sizeof(i->name) - 1] = 0;
  i->grams = (float)a_grams;
  ui_switch_page(page_edit_create());
}

static void am_timer_cb(lv_timer_t *t) {
  char w[16], b[40];
  ui_fmt_weight(w, sizeof(w), scale_net());
  snprintf(b, sizeof(b), T("Waage: %s g  (BOOT = Tara)"), w);
  ui_label_update(am_live, b);
}

static lv_obj_t *page_amount_create() {
  lv_obj_t *s = ui_screen_create();
  char b[48];
  snprintf(b, sizeof(b), T("Menge · %s"), a_name);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  am_value = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(am_value, LV_ALIGN_CENTER, 0, -60);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_align(m, LV_ALIGN_CENTER, -118, -60);
  lv_obj_add_event_cb(m, am_adj_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_align(p, LV_ALIGN_CENTER, 118, -60);
  lv_obj_add_event_cb(p, am_adj_cb, LV_EVENT_CLICKED, (void *)1);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 0);
  for (int i = 0; i < 3; i++) {
    snprintf(b, sizeof(b), "%d", AM_STEPS[i]);
    am_steps[i] = ui_btn(row, b, BTN_NORMAL);
    lv_obj_set_height(am_steps[i], 42);
    lv_obj_set_style_pad_hor(am_steps[i], 14, 0);
    lv_obj_add_event_cb(am_steps[i], am_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)AM_STEPS[i]);
  }

  am_live = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(am_live, LV_ALIGN_CENTER, 0, 46);

  lv_obj_t *brow = ui_box(s);
  lv_obj_set_size(brow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(brow, 10, 0);
  lv_obj_align(brow, LV_ALIGN_CENTER, 0, 104);
  lv_obj_t *w = ui_btn(brow, "Abwiegen", BTN_NORMAL);
  lv_obj_add_event_cb(w, am_weigh_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ok = ui_btn(brow, "Übernehmen", BTN_PRIMARY);
  lv_obj_add_event_cb(ok, am_ok_cb, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, am_timer_cb, 200);
  am_show();
  am_timer_cb(NULL);
  return s;
}
