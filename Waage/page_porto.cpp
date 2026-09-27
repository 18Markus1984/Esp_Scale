// ============================================================
//  Modus "Porto": zeigt die passende Briefklasse der Deutschen Post
//
//  Die Portoklassen stehen in /Waage/Porto/porto.txt und lassen sich
//  unter System -> Setup -> Porto ändern (Standard: Stand 2026).
//  Zum 01.01.2027 ändert die Post die Formate (u. a. Großbrief bis
//  1000 g, Maxibrief bis 2000 g) -> dann im Setup anpassen.
//  Die Waage prüft nur das Gewicht; Maße und Dicke entscheiden mit!
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "data.h"
#include "ui_text.h"
#include "sound.h"
#include <string.h>
#include <stdio.h>

#define KNAPP_G 3.0f  // ab so wenig Restluft warnen

static lv_obj_t *p_weight, *p_class, *p_margin, *p_toast;
static lv_timer_t *p_timer;
static int p_prev_state;

static void p_timer_cb(lv_timer_t *t) {
  float g = scale_net();
  if (g < 0.5f && g > -5.0f) g = 0.0f;  // leichtes Nullpunkt-Wackeln ausblenden
  char b[48], w[16];
  ui_fmt_weight(w, sizeof(w), g);
  ui_label_update(p_weight, w);

  // passende Klasse suchen
  int k = -1;
  for (int i = 0; i < g_porto_count; i++) {
    if (g <= g_porto[i].max_g) { k = i; break; }
  }

  // 0 = leer, 1 = ok, 2 = knapp, 3 = zu schwer
  int state;
  if (g < 0.5f) state = 0;
  else if (k < 0) state = 3;
  else if (g_porto[k].max_g - g < KNAPP_G) state = 2;
  else state = 1;

  if (state == 0) {
    ui_label_update(p_class, "Brief auflegen");
    ui_label_update(p_margin, "");
  } else if (state == 3) {
    ui_label_update(p_class, "Zu schwer für Brief");
    ui_label_update(p_margin, "Päckchen oder Paket nutzen");
  } else {
    snprintf(b, sizeof(b), "%s · %d,%02d €", g_porto[k].name, g_porto[k].price_ct / 100,
             g_porto[k].price_ct % 100);
    ui_label_update(p_class, b);
    snprintf(b, sizeof(b), T("bis %d g · noch %d g Luft"), g_porto[k].max_g,
             (int)(g_porto[k].max_g - g));
    ui_label_update(p_margin, b);
    snprintf(b, sizeof(b), T("Porto: %s %d,%02d €"), g_porto[k].name, g_porto[k].price_ct / 100, g_porto[k].price_ct % 100);
    if (track_update(g, scale_stable(), b)) ui_toast_show(p_toast, "Im Protokoll gespeichert", C_ACCENT);
  }

  if (state != p_prev_state) {
    p_prev_state = state;
    lv_color_t c = state == 0 ? C_MUTED : state == 1 ? C_ACCENT : state == 2 ? C_WARN : C_DANGER;
    lv_obj_set_style_text_color(p_class, c, 0);
    lv_obj_set_style_text_color(p_margin, state == 2 ? C_WARN : C_MUTED, 0);
  }
}

lv_obj_t *page_porto_create() {
  porto_load();
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *t = ui_label(s, "Porto", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  p_weight = ui_label(s, "0,0", &font_sg_80, C_TEXT);
  lv_obj_align(p_weight, LV_ALIGN_CENTER, 0, -50);

  lv_obj_t *unit = ui_label(s, "g", &font_sg_24, C_MUTED);
  lv_obj_align(unit, LV_ALIGN_CENTER, 0, 10);

  p_class = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(p_class, LV_ALIGN_CENTER, 0, 56);

  p_margin = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(p_margin, LV_ALIGN_CENTER, 0, 92);

  lv_obj_t *note = ui_label(s, "Maße und Dicke beachten\nPreise änderbar im Setup", &font_sg_14, C_FAINT);
  lv_obj_align(note, LV_ALIGN_CENTER, 0, 140);

  p_toast = ui_toast_create(s);
  lv_obj_align(p_toast, LV_ALIGN_CENTER, 0, 140);
  track_reset();
  p_prev_state = -1;
  p_timer = ui_page_timer(s, p_timer_cb, 100);
  p_timer_cb(NULL);
  return s;
}

// ============================================================
//  Setup -> Porto: Klassen bearbeiten
// ============================================================
static int e_idx = -1;       // bearbeitete Klasse, -1 = neu
static porto_t e_draft;      // Entwurf, bis "Speichern"
static int e_step = 10;
static bool e_confirm = false;

static lv_obj_t *page_porto_edit_create();

static void pl_row_cb(lv_event_t *e) {
  e_idx = (int)(intptr_t)lv_event_get_user_data(e);
  e_draft = g_porto[e_idx];
  ui_switch_page(page_porto_edit_create());
}

static void pl_new_cb(lv_event_t *e) {
  if (g_porto_count >= PORTO_MAX) return;
  e_idx = -1;
  memset(&e_draft, 0, sizeof(e_draft));
  snprintf(e_draft.name, sizeof(e_draft.name), T("Neue Klasse"));
  e_draft.max_g = g_porto_count ? g_porto[g_porto_count - 1].max_g * 2 : 20;
  e_draft.price_ct = 100;
  ui_switch_page(page_porto_edit_create());
}

static void pl_done_cb(lv_event_t *e) {
  ui_switch_page(page_setup_back());
}

lv_obj_t *page_porto_setup_create() {
  porto_load();
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Portoklassen", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 44);

  lv_obj_t *list = ui_box(s);
  lv_obj_set_size(list, 290, 190);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, -8);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);  // nötig, damit sich die Liste wischen lässt
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  for (int i = 0; i < g_porto_count; i++) {
    lv_obj_t *row = ui_box(list);
    lv_obj_set_size(row, 290, 46);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, pl_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *n = ui_label(row, g_porto[i].name, &font_sg_18, C_TEXT);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
    char b[32];
    snprintf(b, sizeof(b), "%d g · %d,%02d €", g_porto[i].max_g, g_porto[i].price_ct / 100,
             g_porto[i].price_ct % 100);
    lv_obj_t *v = ui_label(row, b, &font_sg_14, C_MUTED);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 118);
  lv_obj_t *nb = ui_btn(row, "+ Klasse", BTN_NORMAL);
  lv_obj_add_event_cb(nb, pl_new_cb, LV_EVENT_CLICKED, NULL);
  if (g_porto_count >= PORTO_MAX) lv_obj_add_state(nb, LV_STATE_DISABLED);
  lv_obj_t *db = ui_btn(row, "Fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(db, pl_done_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

// ------------------------------------------------------------
//  Klasse bearbeiten: Name, Gewichtsgrenze, Preis
// ------------------------------------------------------------
static lv_obj_t *pe_weight, *pe_price, *pe_steps[3], *pe_del;
static const int PE_STEPS[3] = { 1, 10, 100 };

static void pe_show() {
  char b[24];
  snprintf(b, sizeof(b), T("bis %d g"), e_draft.max_g);
  lv_label_set_text(pe_weight, b);
  snprintf(b, sizeof(b), "%d,%02d €", e_draft.price_ct / 100, e_draft.price_ct % 100);
  lv_label_set_text(pe_price, b);
  for (int i = 0; i < 3; i++) {
    bool on = PE_STEPS[i] == e_step;
    lv_obj_set_style_bg_color(pe_steps[i], on ? C_TEXT : C_SURFACE, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(pe_steps[i], 0), on ? C_BG : C_TEXT2, 0);
  }
}

// user_data: 0 = Gewicht -, 1 = Gewicht +, 2 = Preis -, 3 = Preis +
static void pe_adj_cb(lv_event_t *e) {
  int v = (int)(intptr_t)lv_event_get_user_data(e);
  int d = (v & 1) ? e_step : -e_step;
  if (v < 2) {
    e_draft.max_g += d;
    if (e_draft.max_g < 1) e_draft.max_g = 1;
    if (e_draft.max_g > 99999) e_draft.max_g = 99999;
  } else {
    e_draft.price_ct += d;
    if (e_draft.price_ct < 0) e_draft.price_ct = 0;
  }
  pe_show();
}

static void pe_step_cb(lv_event_t *e) {
  e_step = (int)(intptr_t)lv_event_get_user_data(e);
  pe_show();
}

static void pe_name_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(e_draft.name, text, sizeof(e_draft.name) - 1);
    e_draft.name[sizeof(e_draft.name) - 1] = 0;
  }
  ui_switch_page(page_porto_edit_create());
}

static void pe_name_cb(lv_event_t *e) {
  static const char *const SUGG[] = { "Standardbrief", "Kompaktbrief", "Großbrief", "Maxibrief",
                                      "Päckchen", "Paket", "Postkarte", "Warensendung" };
  ui_switch_page(ui_text_page_create("Portoklasse · Name", e_draft.name, 23, false, SUGG, 8, pe_name_done));
}

static void pe_save_cb(lv_event_t *e) {
  if (e_idx < 0) g_porto[g_porto_count++] = e_draft;
  else g_porto[e_idx] = e_draft;
  porto_save();
  sound_play(SND_SAVE);
  ui_switch_page(page_porto_setup_create());
}

static void pe_delete_cb(lv_event_t *e) {
  if (e_idx < 0) {  // neue Klasse verwerfen
    ui_switch_page(page_porto_setup_create());
    return;
  }
  if (!e_confirm) {
    e_confirm = true;
    lv_label_set_text(lv_obj_get_child(pe_del, 0), "Wirklich?");
    lv_obj_set_style_bg_color(pe_del, C_DANGER, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(pe_del, 0), C_ON_ACCENT, 0);
    return;
  }
  for (int i = e_idx; i < g_porto_count - 1; i++) g_porto[i] = g_porto[i + 1];
  g_porto_count--;
  porto_save();
  ui_switch_page(page_porto_setup_create());
}

static lv_obj_t *adj_row(lv_obj_t *s, lv_coord_t y, int base, lv_obj_t **label) {
  *label = ui_label(s, "", &font_sg_24, C_TEXT);
  lv_obj_align(*label, LV_ALIGN_CENTER, 0, y);
  lv_obj_t *m = ui_round_btn(s, "−", NULL);
  lv_obj_set_size(m, 52, 52);
  lv_obj_align(m, LV_ALIGN_CENTER, -118, y);
  lv_obj_add_event_cb(m, pe_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)base);
  lv_obj_t *p = ui_round_btn(s, "+", NULL);
  lv_obj_set_size(p, 52, 52);
  lv_obj_align(p, LV_ALIGN_CENTER, 118, y);
  lv_obj_add_event_cb(p, pe_adj_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(base + 1));
  return *label;
}

static lv_obj_t *page_porto_edit_create() {
  lv_obj_t *s = ui_screen_create();
  e_confirm = false;
  char b[48];

  snprintf(b, sizeof(b), "%s ›", e_draft.name);
  lv_obj_t *nb = ui_btn(s, b, BTN_NORMAL);
  lv_obj_set_height(nb, 44);
  lv_obj_align(nb, LV_ALIGN_CENTER, 0, -118);
  lv_obj_add_event_cb(nb, pe_name_cb, LV_EVENT_CLICKED, NULL);

  adj_row(s, -56, 0, &pe_weight);
  adj_row(s, 4, 2, &pe_price);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 58);
  for (int i = 0; i < 3; i++) {
    snprintf(b, sizeof(b), "%d", PE_STEPS[i]);
    pe_steps[i] = ui_btn(row, b, BTN_NORMAL);
    lv_obj_set_height(pe_steps[i], 40);
    lv_obj_set_style_pad_hor(pe_steps[i], 14, 0);
    lv_obj_add_event_cb(pe_steps[i], pe_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PE_STEPS[i]);
  }

  lv_obj_t *brow = ui_box(s);
  lv_obj_set_size(brow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(brow, 10, 0);
  lv_obj_align(brow, LV_ALIGN_CENTER, 0, 122);
  pe_del = ui_btn(brow, e_idx < 0 ? "Verwerfen" : "Löschen", BTN_NORMAL);
  lv_obj_set_style_text_color(lv_obj_get_child(pe_del, 0), C_DANGER, 0);
  lv_obj_add_event_cb(pe_del, pe_delete_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sv = ui_btn(brow, "Speichern", BTN_PRIMARY);
  lv_obj_add_event_cb(sv, pe_save_cb, LV_EVENT_CLICKED, NULL);

  pe_show();
  return s;
}
