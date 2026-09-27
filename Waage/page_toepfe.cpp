// ============================================================
//  System -> Töpfe (siehe Design-Sheet)
//   Liste:       Antippen = Topf abziehen, gedrückt halten = bearbeiten
//   Anlegen:     leeren Topf auflegen, Farbe, Name über den Ring
//   Konflikt:    Warnung bei ähnlichem Gewicht
//   Bearbeiten:  Farbe, Umbenennen, Neu wiegen, Löschen
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_text.h"
#include "scale.h"
#include "settings.h"
#include "storage.h"
#include "data.h"
#include "sound.h"
#include <stdio.h>
#include <string.h>

// Vorschläge für die Ringeingabe
static const char *const KITCHEN[] = {
  "Topf", "Großer Topf", "Kleiner Topf", "Kochtopf", "Milchtopf", "Pfanne", "Schüssel",
  "Rührschüssel", "Salatschüssel", "Schale", "Auflaufform", "Backform", "Bräter", "Wok",
  "Messbecher", "Teller", "Dose", "Brotdose", "Glas", "Kanne",
};
static const int KITCHEN_N = sizeof(KITCHEN) / sizeof(KITCHEN[0]);

static lv_obj_t *page_list_create();
static lv_obj_t *page_new_create();
static lv_obj_t *page_edit_create();

// Zustand, bleibt über Seitenwechsel (z. B. Ringeingabe) erhalten
static int e_idx = -1;         // -1 = neuer Topf, sonst Index
static bool n_reweigh = false; // Neu wiegen eines vorhandenen Topfs
static int n_color = 0;
static char n_name[32];
static bool n_name_custom = false;  // false = automatischer Name "Topf N"
static float n_pending = 0;    // Gewicht, das nach dem Konflikt gespeichert wird

// ------------------------------------------------------------
//  Gemeinsam: Farbauswahl
// ------------------------------------------------------------
static lv_obj_t *color_dots[POT_COLORS];

static void color_dots_update(int selected) {
  for (int i = 0; i < POT_COLORS; i++)
    lv_obj_set_style_border_width(color_dots[i], i == selected ? 3 : 0, 0);
}

static lv_obj_t *color_row(lv_obj_t *parent, int selected, lv_event_cb_t cb) {
  lv_obj_t *row = ui_box(parent);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 14, 0);
  for (int i = 0; i < POT_COLORS; i++) {
    lv_obj_t *d = ui_box(row);
    lv_obj_set_size(d, 34, 34);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, lv_color_hex(pot_color_hex(i)), 0);
    lv_obj_set_style_border_color(d, C_TEXT, 0);
    lv_obj_add_flag(d, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(d, 6);
    lv_obj_add_event_cb(d, cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    color_dots[i] = d;
  }
  color_dots_update(selected);
  return row;
}

static void today(char out[11]) {
  log_today(out);
  if (!out[0]) strcpy(out, "");
}

// ------------------------------------------------------------
//  Liste
// ------------------------------------------------------------
static void list_click_cb(lv_event_t *e) {
  ui_pot_subtract((int)(intptr_t)lv_event_get_user_data(e));
}

static void list_long_cb(lv_event_t *e) {
  e_idx = (int)(intptr_t)lv_event_get_user_data(e);
  ui_switch_page(page_edit_create());
}

static void list_new_cb(lv_event_t *e) {
  if (g_pot_count >= POT_MAX) return;
  e_idx = -1;
  n_reweigh = false;
  n_color = g_pot_count % POT_COLORS;
  snprintf(n_name, sizeof(n_name), T("Topf %d"), g_pot_count + 1);
  n_name_custom = false;
  scale_tare();  // Waage ist leer -> Nullpunkt setzen
  ui_switch_page(page_new_create());
}

static lv_obj_t *page_list_create() {
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *t = ui_label(s, "Töpfe", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 290, 176);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -8);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  if (!storage_ok()) {
    lv_obj_t *l = ui_label(list, "Keine SD-Karte", &font_sg_18, C_WARN);
    lv_obj_set_width(l, 290);
  } else if (g_pot_count == 0) {
    lv_obj_t *l = ui_label(list, "Noch keine Töpfe", &font_sg_18, C_MUTED);
    lv_obj_set_width(l, 290);
  }

  for (int i = 0; i < g_pot_count; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 290, 50);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, list_click_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(row, list_long_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);

    lv_obj_t *dot = ui_box(row);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(pot_color_hex(g_pots[i].color)), 0);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t *name = ui_label(row, g_pots[i].name, &font_sg_18, C_TEXT);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 170);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 26, 0);

    char g[16];
    snprintf(g, sizeof(g), "%d g", (int)(g_pots[i].grams + 0.5f));
    lv_obj_t *w = ui_label(row, g, &font_sg_18, C_MUTED);
    lv_obj_align(w, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  lv_obj_t *hint = ui_label(s, "Antippen: abziehen · Halten: bearbeiten", &font_sg_14, C_FAINT);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 96);

  lv_obj_t *b = ui_btn(s, "+ Neuer Topf", BTN_PRIMARY);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 142);
  lv_obj_add_event_cb(b, list_new_cb, LV_EVENT_CLICKED, NULL);
  if (!storage_ok() || g_pot_count >= POT_MAX) lv_obj_add_state(b, LV_STATE_DISABLED);
  return s;
}

lv_obj_t *page_toepfe_create() {
  pots_load();
  return page_list_create();
}

// ------------------------------------------------------------
//  Anlegen / Neu wiegen
// ------------------------------------------------------------
static lv_obj_t *nw_weight, *nw_state, *nw_toast;
static lv_timer_t *nw_timer;

static void commit(float grams) {
  if (e_idx < 0) {
    pot_t *p = &g_pots[g_pot_count];
    memset(p, 0, sizeof(*p));
    strncpy(p->name, n_name, sizeof(p->name) - 1);
    p->grams = grams;
    p->color = (uint8_t)n_color;
    today(p->created);
    g_pot_count++;
  } else {
    g_pots[e_idx].grams = grams;
    g_pots[e_idx].color = (uint8_t)n_color;
  }
  pots_save();
  sound_play(SND_SAVE);
  ui_pots_changed();
  ui_switch_page(page_list_create());
}

static lv_obj_t *page_conflict_create(int other);

static void nw_save_cb(lv_event_t *e) {
  float g = scale_net();
  if (!scale_stable() || g < 5.0f) {
    ui_toast_show(nw_toast, "Noch nicht stabil", C_WARN);
    return;
  }
  int other = pot_match(g, (float)g_set.tol_g, e_idx);
  n_pending = g;
  if (other >= 0) ui_switch_page(page_conflict_create(other));
  else commit(g);
}

static void name_done(bool ok, const char *text) {
  if (ok && text[0]) {  // leer bestätigt = automatischen Namen behalten
    strncpy(n_name, text, sizeof(n_name) - 1);
    n_name[sizeof(n_name) - 1] = 0;
    n_name_custom = true;
  }
  ui_switch_page(page_new_create());
}

static void nw_name_cb(lv_event_t *e) {
  ui_switch_page(ui_text_page_create("Neuer Topf · Name", n_name_custom ? n_name : "", 31, false, KITCHEN,
                                     KITCHEN_N, name_done));
}

static void nw_color_cb(lv_event_t *e) {
  n_color = (int)(intptr_t)lv_event_get_user_data(e);
  color_dots_update(n_color);
}

static void nw_timer_cb(lv_timer_t *t) {
  char w[16], b[24];
  ui_fmt_weight(w, sizeof(w), scale_net());
  snprintf(b, sizeof(b), "%s g", w);
  ui_label_update(nw_weight, b);
  bool st = scale_stable();
  ui_chip_set(nw_state, st ? "stabil" : "misst …", st ? C_ACCENT : C_MUTED, false);
}

static lv_obj_t *page_new_create() {
  lv_obj_t *s = ui_screen_create();
  char b[48];

  if (n_reweigh) snprintf(b, sizeof(b), T("Neu wiegen · %s"), n_name);
  else snprintf(b, sizeof(b), T("Leeren Topf auflegen"));
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -138);

  nw_weight = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(nw_weight, LV_ALIGN_CENTER, 0, -96);

  nw_state = ui_chip(s, "misst …", C_MUTED);
  lv_obj_align(nw_state, LV_ALIGN_CENTER, 0, -56);

  lv_obj_t *colors = color_row(s, n_color, nw_color_cb);
  lv_obj_align(colors, LV_ALIGN_CENTER, 0, -12);

  if (!n_reweigh) {
    snprintf(b, sizeof(b), T("Name: %s ›"), n_name);
    lv_obj_t *nb = ui_btn(s, b, BTN_NORMAL);
    lv_obj_set_height(nb, 44);
    lv_obj_set_style_text_font(lv_obj_get_child(nb, 0), &font_sg_14, 0);
    lv_obj_align(nb, LV_ALIGN_CENTER, 0, 38);
    lv_obj_add_event_cb(nb, nw_name_cb, LV_EVENT_CLICKED, NULL);
  }

  lv_obj_t *save = ui_btn(s, "Speichern", BTN_PRIMARY);
  lv_obj_align(save, LV_ALIGN_CENTER, 0, 96);
  lv_obj_add_event_cb(save, nw_save_cb, LV_EVENT_CLICKED, NULL);

  nw_toast = ui_toast_create(s);
  lv_obj_align(nw_toast, LV_ALIGN_CENTER, 0, 148);

  nw_timer = ui_page_timer(s, nw_timer_cb, 100);
  nw_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Konflikt: ähnlicher Topf
// ------------------------------------------------------------
static void cf_cancel_cb(lv_event_t *e) {
  ui_switch_page(page_new_create());
}

static void cf_ok_cb(lv_event_t *e) {
  commit(n_pending);
}

static lv_obj_t *page_conflict_create(int other) {
  lv_obj_t *s = ui_screen_create();
  char b[80];

  lv_obj_t *h = ui_label(s, "Ähnlicher Topf", &font_sg_24, C_WARN);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -100);

  snprintf(b, sizeof(b), "%d g", (int)(n_pending + 0.5f));
  lv_obj_t *w = ui_label(s, b, &font_sg_34, C_TEXT);
  lv_obj_align(w, LV_ALIGN_CENTER, 0, -54);

  snprintf(b, sizeof(b), T("liegt nah an „%s“ (%d g)"), g_pots[other].name, (int)(g_pots[other].grams + 0.5f));
  lv_obj_t *l = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_set_width(l, 290);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, -6);

  lv_obj_t *l2 = ui_label(s, "Erkennung kann beide verwechseln", &font_sg_14, C_FAINT);
  lv_obj_align(l2, LV_ALIGN_CENTER, 0, 34);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 96);
  lv_obj_t *c = ui_btn(row, "Abbrechen", BTN_NORMAL);
  lv_obj_add_event_cb(c, cf_cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *o = ui_btn(row, "Trotzdem", BTN_PRIMARY);
  lv_obj_add_event_cb(o, cf_ok_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

// ------------------------------------------------------------
//  Bearbeiten
// ------------------------------------------------------------
static lv_obj_t *ed_del;
static bool ed_confirm = false;

static void ed_color_cb(lv_event_t *e) {
  g_pots[e_idx].color = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  color_dots_update(g_pots[e_idx].color);
  pots_save();
  ui_pots_changed();
}

static void rename_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(g_pots[e_idx].name, text, sizeof(g_pots[e_idx].name) - 1);
    g_pots[e_idx].name[sizeof(g_pots[e_idx].name) - 1] = 0;
    pots_save();
    ui_pots_changed();
  }
  ui_switch_page(page_edit_create());
}

static void ed_rename_cb(lv_event_t *e) {
  ui_switch_page(ui_text_page_create("Topf umbenennen", g_pots[e_idx].name, 31, false, KITCHEN,
                                     KITCHEN_N, rename_done));
}

static void ed_reweigh_cb(lv_event_t *e) {
  n_reweigh = true;
  n_color = g_pots[e_idx].color;
  strncpy(n_name, g_pots[e_idx].name, sizeof(n_name) - 1);
  n_name[sizeof(n_name) - 1] = 0;
  scale_tare();
  ui_switch_page(page_new_create());
}

static void ed_delete_cb(lv_event_t *e) {
  if (!ed_confirm) {  // erst beim zweiten Tippen wirklich löschen
    ed_confirm = true;
    lv_label_set_text(lv_obj_get_child(ed_del, 0), "Wirklich löschen?");
    lv_obj_set_style_bg_color(ed_del, C_DANGER, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(ed_del, 0), C_ON_ACCENT, 0);
    return;
  }
  for (int i = e_idx; i < g_pot_count - 1; i++) g_pots[i] = g_pots[i + 1];
  g_pot_count--;
  pots_save();
  ui_pots_changed();
  ui_switch_page(page_list_create());
}

static lv_obj_t *page_edit_create() {
  lv_obj_t *s = ui_screen_create();
  pot_t *p = &g_pots[e_idx];
  ed_confirm = false;
  char b[64];

  lv_obj_t *h = ui_label(s, p->name, &font_sg_24, C_TEXT);
  lv_obj_align(h, LV_ALIGN_CENTER, 0, -112);

  if (strlen(p->created) == 10)
    snprintf(b, sizeof(b), T("%d g · angelegt %.2s.%.2s."), (int)(p->grams + 0.5f), p->created + 8, p->created + 5);
  else snprintf(b, sizeof(b), "%d g", (int)(p->grams + 0.5f));
  lv_obj_t *info = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(info, LV_ALIGN_CENTER, 0, -80);

  lv_obj_t *colors = color_row(s, p->color, ed_color_cb);
  lv_obj_align(colors, LV_ALIGN_CENTER, 0, -34);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 30);
  lv_obj_t *rn = ui_btn(row, "Umbenennen", BTN_NORMAL);
  lv_obj_set_style_pad_hor(rn, 18, 0);
  lv_obj_add_event_cb(rn, ed_rename_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *rw = ui_btn(row, "Neu wiegen", BTN_NORMAL);
  lv_obj_set_style_pad_hor(rw, 18, 0);
  lv_obj_add_event_cb(rw, ed_reweigh_cb, LV_EVENT_CLICKED, NULL);

  ed_del = ui_btn(s, "Löschen", BTN_NORMAL);
  lv_obj_set_style_border_color(ed_del, lv_color_hex(0x6B2E29), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(ed_del, 0), C_DANGER, 0);
  lv_obj_align(ed_del, LV_ALIGN_CENTER, 0, 96);
  lv_obj_add_event_cb(ed_del, ed_delete_cb, LV_EVENT_CLICKED, NULL);
  return s;
}
