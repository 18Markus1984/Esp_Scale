// ============================================================
//  Modi -> Cocktail
//  Wie der Rezeptmodus, aber in Millilitern: Cocktailrezepte stehen
//  in ml, gewogen wird in Gramm. Die Umrechnung läuft über die Dichte
//  der Zutat (Sirup ist schwerer als Wasser, Spirituosen leichter).
//  Rezepte liegen in /Waage/Cocktails/*.txt
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "scale.h"
#include "storage.h"
#include "data.h"
#include "sound.h"
#include "settings.h"
#include "hal.h"
#include "config.h"
#include "ui_text.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LIST_MAX 20

static char c_files[LIST_MAX][48], c_names[LIST_MAX][40];
static int c_counts[LIST_MAX];
static recipe_t c_rec;     // gewähltes Rezept (Mengen in ml)
static int c_glasses = 1;  // wie viele Gläser
static int c_step = 0;
static float c_done_g[RECIPE_MAX_ING];

static lv_obj_t *page_prev_create();
static lv_obj_t *page_step_create();

static float step_ml(int i) {
  return c_rec.ing[i].grams * c_glasses / (c_rec.portions > 0 ? c_rec.portions : 1);
}

static float step_g(int i) {
  return step_ml(i) * ingredient_density(c_rec.ing[i].name);
}

// ------------------------------------------------------------
//  Liste
// ------------------------------------------------------------
static lv_obj_t *page_edit_create();
static recipe_t c_draft;  // neuer Cocktail, bis "Speichern"

static void pick_cb(int index) {
  if (index == 0) {  // "+ Neuer Cocktail"
    memset(&c_draft, 0, sizeof(c_draft));
    c_draft.portions = 1;
    ui_switch_page(page_edit_create());
    return;
  }
  index--;
  if (!recipe_load_dir(DIR_COCKTAILS, c_files[index], &c_rec)) return;
  c_glasses = 1;
  ui_switch_page(page_prev_create());
}

lv_obj_t *page_cocktail_create() {
  lv_obj_t *s = ui_screen_create();
  if (!storage_ok()) {
    lv_obj_t *l = ui_label(s, "Keine SD-Karte", &font_sg_18, C_MUTED);
    lv_obj_center(l);
    return s;
  }
  int n = recipes_list_dir(DIR_COCKTAILS, c_files, c_names, c_counts, LIST_MAX);
  static char subs[LIST_MAX][16];
  static ui_list_item_t items[LIST_MAX + 1];
  items[0].title = "+ Neuer Cocktail";
  items[0].sub = "anlegen";
  for (int i = 0; i < n; i++) {
    snprintf(subs[i], sizeof(subs[i]), T("%d Zutaten"), c_counts[i]);
    items[i + 1].title = c_names[i];
    items[i + 1].sub = subs[i];
  }
  lv_obj_t *list = ui_curved_list(s, items, n + 1, pick_cb);
  if (n > 0) lv_obj_scroll_to_view(lv_obj_get_child(list, 1), LV_ANIM_OFF);

  lv_obj_t *t = ui_label(s, "Cocktails · SD-Karte", &font_sg_14, C_MUTED);
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
static lv_obj_t *pv_glasses, *pv_rows[RECIPE_MAX_ING];

static void pv_show() {
  char b[24];
  if (c_glasses == 1) snprintf(b, sizeof(b), T("1 Glas"));
  else snprintf(b, sizeof(b), T("%d Gläser"), c_glasses);
  lv_label_set_text(pv_glasses, b);
  for (int i = 0; i < c_rec.count; i++) {
    snprintf(b, sizeof(b), T("%d ml"), (int)lroundf(step_ml(i)));
    lv_label_set_text(pv_rows[i], b);
  }
}

static void pv_adj_cb(lv_event_t *e) {
  c_glasses += (int)(intptr_t)lv_event_get_user_data(e) ? 1 : -1;
  if (c_glasses < 1) c_glasses = 1;
  if (c_glasses > 6) c_glasses = 6;
  pv_show();
}

static void pv_start_cb(lv_event_t *e) {
  c_step = 0;
  memset(c_done_g, 0, sizeof(c_done_g));
  scale_tare();
  ui_switch_page(page_step_create());
}

static lv_obj_t *page_prev_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, c_rec.name, &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 44);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 250, 120);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -34);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  for (int i = 0; i < c_rec.count; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 250, 30);
    lv_obj_t *n = ui_label(row, c_rec.ing[i].name, &font_sg_18, C_TEXT);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
    pv_rows[i] = ui_label(row, "", &font_sg_18, C_MUTED);
    lv_obj_align(pv_rows[i], LV_ALIGN_RIGHT_MID, 0, 0);
  }

  pv_glasses = ui_label(s, "", &font_sg_18, C_TEXT);
  lv_obj_align(pv_glasses, LV_ALIGN_CENTER, 0, 46);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_set_size(m, 46, 46);
  lv_obj_align(m, LV_ALIGN_CENTER, -100, 46);
  lv_obj_add_event_cb(m, pv_adj_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_set_size(p, 46, 46);
  lv_obj_align(p, LV_ALIGN_CENTER, 100, 46);
  lv_obj_add_event_cb(p, pv_adj_cb, LV_EVENT_CLICKED, (void *)1);

  lv_obj_t *go = ui_btn(s, "Mixen", BTN_PRIMARY);
  lv_obj_set_style_pad_hor(go, 34, 0);
  lv_obj_align(go, LV_ALIGN_CENTER, 0, 112);
  lv_obj_add_event_cb(go, pv_start_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *h = ui_label(s, "Glas aufs Podest, dann Mixen", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 154);

  pv_show();
  return s;
}

// ------------------------------------------------------------
//  Schritt: eine Zutat eingießen
// ------------------------------------------------------------
static lv_obj_t *st_ring, *st_val, *st_sub, *st_state, *st_toast;
static int st_prev_state;

static void st_finish() {
  float sum_g = 0, sum_ml = 0;
  for (int i = 0; i < c_rec.count; i++) {
    sum_g += c_done_g[i];
    sum_ml += c_done_g[i] / ingredient_density(c_rec.ing[i].name);
  }
  sound_parking_reset();
  sound_play(SND_DONE);
  char note[48];
  snprintf(note, sizeof(note), T("Cocktail: %.24s"), c_rec.name);
  log_add(sum_g, note);

  lv_obj_t *s = ui_screen_create();
  lv_obj_t *ring = ui_ring(s, 396);
  lv_arc_set_value(ring, 1000);
  lv_obj_t *t = ui_label(s, "Fertig", &font_sg_24, C_ACCENT);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -70);
  lv_obj_t *n = ui_label(s, c_rec.name, &font_sg_34, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -20);
  char b[48];
  snprintf(b, sizeof(b), T("%d ml · %d %s"), (int)lroundf(sum_ml), c_glasses, c_glasses > 1 ? T("Gläser") : T("Glas"));
  lv_obj_t *v = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 24);
  lv_obj_t *h = ui_label(s, "Umrühren, Eis dazu, fertig", &font_sg_14, C_FAINT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 56);
  lv_obj_t *b2 = ui_btn(s, "Noch einen", BTN_PRIMARY);
  lv_obj_align(b2, LV_ALIGN_CENTER, 0, 110);
  lv_obj_add_event_cb(b2, [](lv_event_t *e) { ui_switch_page(page_prev_create()); }, LV_EVENT_CLICKED, NULL);
  ui_switch_page(s);
}

static void st_next_cb(lv_event_t *e);

static void st_next_cb(lv_event_t *e) {
  c_done_g[c_step] = scale_net();
  if (c_step + 1 >= c_rec.count) {
    st_finish();
    return;
  }
  c_step++;
  scale_tare();
  ui_switch_page(page_step_create());
}

static void st_back_cb(lv_event_t *e) {
  sound_parking_reset();
  if (c_step == 0) {
    ui_switch_page(page_prev_create());
    return;
  }
  c_step--;
  scale_tare();
  ui_switch_page(page_step_create());
}

static uint32_t st_ok_since = 0;

static void st_tara_cb(lv_event_t *e) {
  scale_tare();
  sound_play(SND_TARA);
}

// fertig eingegossen und ruhig -> automatisch zur nächsten Zutat
static void st_auto(int state) {
  if (!g_set.auto_next || state == 0 || !scale_stable()) {
    st_ok_since = 0;
    return;
  }
  uint32_t now = hal_millis();
  if (st_ok_since == 0) st_ok_since = now;
  if (now - st_ok_since < 1200) return;
  st_ok_since = 0;
  st_next_cb(NULL);
}

static void st_timer_cb(lv_timer_t *t) {
  float goal_g = step_g(c_step);
  float dens = ingredient_density(c_rec.ing[c_step].name);
  float g = scale_net();
  float ml = g / dens;
  float tol = fmaxf(1.0f, step_ml(c_step) * 0.03f) * dens;  // 3 %, mindestens 1 ml

  char b[32];
  snprintf(b, sizeof(b), "%d", (int)lroundf(ml));
  ui_label_update(st_val, b);

  int v = (int)(g / goal_g * 1000.0f);
  if (v < 0) v = 0;
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(st_ring) != v) lv_arc_set_value(st_ring, v);

  int state = g < goal_g - tol ? 0 : (g <= goal_g + tol ? 1 : 2);
  if (state != st_prev_state) {
    st_prev_state = state;
    lv_color_t c = state == 0 ? C_TEXT : (state == 1 ? C_ACCENT : C_DANGER);
    lv_obj_set_style_text_color(st_val, c, 0);
    lv_obj_set_style_arc_color(st_ring, c, LV_PART_INDICATOR);
    if (state == 0) ui_label_update(st_state, "eingießen");
    else if (state == 1) ui_label_update(st_state, "passt");
    else ui_label_update(st_state, "zu viel");
    lv_obj_set_style_text_color(st_state, c, 0);
  }
  sound_parking(g, goal_g, tol);  // Parkpiepser wie beim Rezept
  st_auto(state);
}

static lv_obj_t *page_step_create() {
  lv_obj_t *s = ui_screen_create();
  st_ring = ui_ring(s, 396);

  char b[48];
  snprintf(b, sizeof(b), T("Schritt %d / %d · %.16s"), c_step + 1, c_rec.count, c_rec.name);
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t *n = ui_label(s, c_rec.ing[c_step].name, &font_sg_24, C_TEXT);
  lv_obj_align(n, LV_ALIGN_CENTER, 0, -76);

  st_val = ui_label(s, "0", &font_sg_80, C_TEXT);
  lv_obj_align(st_val, LV_ALIGN_CENTER, 0, -14);
  lv_obj_t *u = ui_label(s, "ml", &font_sg_18, C_MUTED);
  lv_obj_align(u, LV_ALIGN_CENTER, 0, 34);

  snprintf(b, sizeof(b), T("von %d ml"), (int)lroundf(step_ml(c_step)));
  st_sub = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(st_sub, LV_ALIGN_CENTER, 0, 62);
  st_state = ui_label(s, "eingießen", &font_sg_14, C_MUTED);
  lv_obj_align(st_state, LV_ALIGN_CENTER, 0, 88);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 130);
  lv_obj_t *tara = ui_btn(row, "Tara", BTN_NORMAL);
  lv_obj_set_style_pad_hor(tara, 14, 0);
  lv_obj_add_event_cb(tara, st_tara_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *bk = ui_btn(row, "Zurück", BTN_NORMAL);
  lv_obj_set_style_pad_hor(bk, 14, 0);
  lv_obj_add_event_cb(bk, st_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *nx = ui_btn(row, c_step + 1 >= c_rec.count ? "Fertig" : "Weiter", BTN_PRIMARY);
  lv_obj_add_event_cb(nx, st_next_cb, LV_EVENT_CLICKED, NULL);

  st_toast = ui_toast_create(s);
  lv_obj_align(st_toast, LV_ALIGN_CENTER, 0, 160);

  st_prev_state = -1;
  sound_parking_reset();
  ui_page_timer(s, st_timer_cb, 100);
  st_timer_cb(NULL);
  return s;
}

// ============================================================
//  Neuen Cocktail anlegen (Name und Zutaten über den Ring,
//  Menge einstellen oder direkt abmessen)
// ============================================================
static const char *const COCKTAIL_ZUTATEN[] = {
  "Gin", "Wodka", "Rum weiß", "Rum braun", "Tequila", "Whisky", "Cachaca", "Aperol", "Campari", "Wermut",
  "Triple Sec", "Kokoslikör", "Kaffeelikör", "Prosecco", "Tonic Water", "Cola", "Ginger Beer", "Soda",
  "Orangensaft", "Ananassaft", "Maracujasaft", "Cranberrysaft", "Limettensaft", "Zitronensaft",
  "Zuckersirup", "Grenadine", "Holundersirup", "Sahne", "Milch", "Espresso",
};
static const int ZUTATEN_N = sizeof(COCKTAIL_ZUTATEN) / sizeof(COCKTAIL_ZUTATEN[0]);

static char e_name[32];
static int e_ml = 40;
static int e_step = 10;
static lv_obj_t *e_toast;

static lv_obj_t *page_amount_create();

// ---------- Name ----------
static void ed_name_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(c_draft.name, text, sizeof(c_draft.name) - 1);
    c_draft.name[sizeof(c_draft.name) - 1] = 0;
  }
  ui_switch_page(page_edit_create());
}

static void ed_name_cb(lv_event_t *e) {
  ui_switch_page(ui_text_page_create("Cocktail · Name", c_draft.name, 39, false, NULL, 0, ed_name_done));
}

// ---------- Zutat ----------
static void ed_ing_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(e_name, text, sizeof(e_name) - 1);
    e_name[sizeof(e_name) - 1] = 0;
    ui_switch_page(page_amount_create());
  } else {
    ui_switch_page(page_edit_create());
  }
}

static void ed_add_cb(lv_event_t *e) {
  if (c_draft.count >= RECIPE_MAX_ING) return;
  ui_switch_page(ui_text_page_create("Zutat", "", 31, false, COCKTAIL_ZUTATEN, ZUTATEN_N, ed_ing_done));
}

static void ed_remove_cb(lv_event_t *e) {  // gedrückt halten
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  for (int k = i; k < c_draft.count - 1; k++) c_draft.ing[k] = c_draft.ing[k + 1];
  c_draft.count--;
  ui_switch_page(page_edit_create());
}

static void ed_save_cb(lv_event_t *e) {
  if (!c_draft.name[0]) {
    ui_toast_show(e_toast, "Bitte zuerst einen Namen", C_WARN);
    return;
  }
  if (c_draft.count == 0) {
    ui_toast_show(e_toast, "Noch keine Zutaten", C_WARN);
    return;
  }
  if (!recipe_save_dir(DIR_COCKTAILS, &c_draft, NULL, 0)) {
    sound_play(SND_WARN);
    ui_toast_show(e_toast, "Speichern fehlgeschlagen", C_WARN);
    return;
  }
  sound_play(SND_SAVE);
  ui_switch_page(page_cocktail_create());
}

static lv_obj_t *page_edit_create() {
  lv_obj_t *s = ui_screen_create();
  char b[64];

  snprintf(b, sizeof(b), "%s ›", c_draft.name[0] ? c_draft.name : T("Name eingeben"));
  lv_obj_t *nb = ui_btn(s, b, BTN_NORMAL);
  lv_obj_set_height(nb, 44);
  lv_obj_align(nb, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_add_event_cb(nb, ed_name_cb, LV_EVENT_CLICKED, NULL);

  int total = 0;
  for (int i = 0; i < c_draft.count; i++) total += (int)lroundf(c_draft.ing[i].grams);
  snprintf(b, sizeof(b), T("%d ml im Glas"), total);
  lv_obj_t *sum = ui_label(s, b, &font_sg_18, C_ACCENT);
  lv_obj_align(sum, LV_ALIGN_CENTER, 0, -82);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 270, 120);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -6);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  if (c_draft.count == 0) ui_label(list, "Noch keine Zutaten", &font_sg_18, C_MUTED);
  for (int i = 0; i < c_draft.count; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 270, 36);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_add_event_cb(row, ed_remove_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    lv_obj_t *n = ui_label(row, c_draft.ing[i].name, &font_sg_18, C_TEXT);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
    snprintf(b, sizeof(b), T("%d ml"), (int)lroundf(c_draft.ing[i].grams));
    lv_obj_t *g = ui_label(row, b, &font_sg_18, C_MUTED);
    lv_obj_align(g, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  lv_obj_t *hint = ui_label(s, "Halten: Zutat entfernen", &font_sg_14, C_FAINT);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 66);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 112);
  lv_obj_t *add = ui_btn(row, "+ Zutat", BTN_NORMAL);
  lv_obj_add_event_cb(add, ed_add_cb, LV_EVENT_CLICKED, NULL);
  if (c_draft.count >= RECIPE_MAX_ING) lv_obj_add_state(add, LV_STATE_DISABLED);
  lv_obj_t *sv = ui_btn(row, "Speichern", BTN_PRIMARY);
  lv_obj_add_event_cb(sv, ed_save_cb, LV_EVENT_CLICKED, NULL);

  e_toast = ui_toast_create(s);
  lv_obj_align(e_toast, LV_ALIGN_CENTER, 0, 158);
  return s;
}

// ---------- Menge der Zutat ----------
static lv_obj_t *am_val, *am_live, *am_steps[3];
static const int AM_STEPS[3] = { 1, 10, 50 };

static void am_show() {
  char b[16];
  snprintf(b, sizeof(b), T("%d ml"), e_ml);
  lv_label_set_text(am_val, b);
  for (int i = 0; i < 3; i++) {
    bool on = AM_STEPS[i] == e_step;
    lv_obj_set_style_bg_color(am_steps[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(am_steps[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

static void am_adj_cb(lv_event_t *e) {
  e_ml += (int)(intptr_t)lv_event_get_user_data(e) ? e_step : -e_step;
  if (e_ml < 1) e_ml = 1;
  if (e_ml > 999) e_ml = 999;
  am_show();
}

static void am_step_cb(lv_event_t *e) {
  e_step = (int)(intptr_t)lv_event_get_user_data(e);
  am_show();
}

static void am_weigh_cb(lv_event_t *e) {  // eingegossene Menge übernehmen
  float ml = scale_net() / ingredient_density(e_name);
  if (ml >= 1.0f) {
    e_ml = (int)lroundf(ml);
    sound_play(SND_TARA);
    am_show();
  }
}

static void am_ok_cb(lv_event_t *e) {
  ingredient_t *i = &c_draft.ing[c_draft.count++];
  strncpy(i->name, e_name, sizeof(i->name) - 1);
  i->name[sizeof(i->name) - 1] = 0;
  i->grams = (float)e_ml;  // in ml
  ui_switch_page(page_edit_create());
}

static void am_timer_cb(lv_timer_t *t) {
  char b[48];
  snprintf(b, sizeof(b), T("Waage: %d ml  (BOOT = Tara)"), (int)lroundf(scale_net() / ingredient_density(e_name)));
  ui_label_update(am_live, b);
}

static lv_obj_t *page_amount_create() {
  lv_obj_t *s = ui_screen_create();
  char b[48];
  snprintf(b, sizeof(b), "%.20s", e_name);
  lv_obj_t *t = ui_label(s, b, &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 46);
  snprintf(b, sizeof(b), T("Dichte %.2f g/ml"), ingredient_density(e_name));
  lv_obj_t *d = ui_label(s, b, &font_sg_14, C_FAINT);
  lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 78);

  am_val = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(am_val, LV_ALIGN_CENTER, 0, -50);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_align(m, LV_ALIGN_CENTER, -118, -50);
  lv_obj_add_event_cb(m, am_adj_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_align(p, LV_ALIGN_CENTER, 118, -50);
  lv_obj_add_event_cb(p, am_adj_cb, LV_EVENT_CLICKED, (void *)1);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 6);
  for (int i = 0; i < 3; i++) {
    snprintf(b, sizeof(b), "%d", AM_STEPS[i]);
    am_steps[i] = ui_btn(row, b, BTN_NORMAL);
    lv_obj_set_height(am_steps[i], 42);
    lv_obj_set_style_pad_hor(am_steps[i], 14, 0);
    lv_obj_add_event_cb(am_steps[i], am_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)AM_STEPS[i]);
  }

  am_live = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(am_live, LV_ALIGN_CENTER, 0, 52);

  lv_obj_t *brow = ui_box(s);
  lv_obj_set_size(brow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(brow, 10, 0);
  lv_obj_align(brow, LV_ALIGN_CENTER, 0, 110);
  lv_obj_t *w = ui_btn(brow, "Abmessen", BTN_NORMAL);
  lv_obj_add_event_cb(w, am_weigh_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ok = ui_btn(brow, "Übernehmen", BTN_PRIMARY);
  lv_obj_add_event_cb(ok, am_ok_cb, LV_EVENT_CLICKED, NULL);

  ui_page_timer(s, am_timer_cb, 200);
  am_show();
  am_timer_cb(NULL);
  return s;
}
