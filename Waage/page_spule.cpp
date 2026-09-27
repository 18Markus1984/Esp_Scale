// ============================================================
//  Modi -> Spule: Restmeter Filament auf einer Spule
//   1. Material wählen (Dichte)
//   2. Leerspule wählen oder neu abwiegen (/Waage/Spulen/spulen.txt)
//   3. Ergebnis: Netto-Gewicht und Restlänge, live
//  Länge = Gewicht / (Dichte * Querschnitt)
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_text.h"
#include "scale.h"
#include "storage.h"
#include "data.h"
#include "sound.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct {
  const char *name;
  float density;  // g/cm³
} material_t;

static const material_t MATERIALS[] = {
  { "PLA", 1.24f }, { "PETG", 1.27f }, { "ABS", 1.04f }, { "ASA", 1.07f },
  { "TPU", 1.21f }, { "PC", 1.20f },   { "Nylon", 1.14f },
};
static const int MAT_N = sizeof(MATERIALS) / sizeof(MATERIALS[0]);

// Auswahl bleibt für das nächste Mal erhalten
static int s_mat = 0;
static int s_spool = -1;          // -1 = ohne Leerspule
static float s_diam = 1.75f;      // mm

static lv_obj_t *page_spool_pick_create();
static lv_obj_t *page_result_create();
static lv_obj_t *page_spool_new_create();

static float spool_weight() {
  return (s_spool >= 0 && s_spool < g_spool_count) ? g_spools[s_spool].grams : 0.0f;
}

static float length_m(float grams) {
  float r_cm = s_diam / 20.0f;  // mm-Durchmesser -> cm-Radius
  float area = (float)M_PI * r_cm * r_cm;
  return grams / MATERIALS[s_mat].density / area / 100.0f;
}

static lv_obj_t *list_title(lv_obj_t *s, const char *txt) {
  lv_obj_t *t = ui_label(s, txt, &font_sg_14, C_MUTED);
  lv_obj_set_style_bg_color(t, C_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(t, 60, 0);
  lv_obj_set_style_pad_top(t, 26, 0);
  lv_obj_set_style_pad_bottom(t, 10, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
  return t;
}

// ------------------------------------------------------------
//  1. Material
// ------------------------------------------------------------
static void mat_cb(int index) {
  s_mat = index;
  ui_switch_page(page_spool_pick_create());
}

lv_obj_t *page_spule_create() {
  spools_load();
  lv_obj_t *s = ui_screen_create();
  static char subs[MAT_N][24];
  static ui_list_item_t items[MAT_N];
  for (int i = 0; i < MAT_N; i++) {
    char d[12];
    fmt_num(d, sizeof(d), MATERIALS[i].density, 2);
    snprintf(subs[i], sizeof(subs[i]), T("%s g/cm³"), d);
    items[i].title = MATERIALS[i].name;
    items[i].sub = subs[i];
  }
  lv_obj_t *list = ui_curved_list(s, items, MAT_N, mat_cb);
  if (s_mat > 0) lv_obj_scroll_to_view(lv_obj_get_child(list, s_mat), LV_ANIM_OFF);
  list_title(s, "Spule · Material");
  return s;
}

// ------------------------------------------------------------
//  2. Leerspule
// ------------------------------------------------------------
static void spool_cb(int index) {
  // 0 = ohne Leerspule, 1..n = gespeicherte, letzte = neu anlegen
  if (index == g_spool_count + 1) {
    if (!storage_ok()) return;
    scale_tare();  // Waage ist leer -> Nullpunkt
    ui_switch_page(page_spool_new_create());
    return;
  }
  s_spool = index - 1;
  ui_switch_page(page_result_create());
}

static lv_obj_t *page_spool_pick_create() {
  lv_obj_t *s = ui_screen_create();
  static char subs[SPOOL_MAX][16];
  static ui_list_item_t items[SPOOL_MAX + 2];
  int n = 0;
  items[n].title = "Ohne Leerspule";
  items[n++].sub = "nur Filament wiegen";
  for (int i = 0; i < g_spool_count; i++) {
    snprintf(subs[i], sizeof(subs[i]), "%d g", (int)lroundf(g_spools[i].grams));
    items[n].title = g_spools[i].name;
    items[n++].sub = subs[i];
  }
  items[n].title = "+ Leerspule";
  items[n++].sub = storage_ok() ? "Waage leer, dann antippen" : "keine SD-Karte";
  lv_obj_t *list = ui_curved_list(s, items, n, spool_cb);
  if (s_spool >= 0 && s_spool < g_spool_count)
    lv_obj_scroll_to_view(lv_obj_get_child(list, s_spool + 1), LV_ANIM_OFF);
  list_title(s, "Spule · Leerspule");
  return s;
}

// ---------- Leerspule anlegen ----------
static char sn_name[24] = "";
static lv_obj_t *sn_weight, *sn_state, *sn_toast;

static void sn_timer_cb(lv_timer_t *t) {
  char w[16], b[24];
  ui_fmt_weight(w, sizeof(w), scale_net());
  snprintf(b, sizeof(b), "%s g", w);
  ui_label_update(sn_weight, b);
  bool st = scale_stable();
  ui_chip_set(sn_state, st ? "stabil" : "misst …", st ? C_ACCENT : C_MUTED, false);
}

static void sn_name_done(bool ok, const char *text) {
  if (ok && text[0]) {
    strncpy(sn_name, text, sizeof(sn_name) - 1);
    sn_name[sizeof(sn_name) - 1] = 0;
  }
  ui_switch_page(page_spool_new_create());
}

static void sn_name_cb(lv_event_t *e) {
  static const char *const SUGG[] = { "Bambu Lab", "Prusament", "Polymaker", "Sunlu", "eSun", "Elegoo",
                                      "Jayo", "Das Filament", "Extrudr", "Pappspule", "Plastikspule" };
  ui_switch_page(ui_text_page_create("Leerspule · Name", sn_name, 23, false, SUGG, 11, sn_name_done));
}

static void sn_save_cb(lv_event_t *e) {
  float g = scale_net();
  if (!scale_stable() || g < 5.0f) {
    ui_toast_show(sn_toast, "Noch nicht stabil", C_WARN);
    return;
  }
  if (!sn_name[0]) {
    ui_toast_show(sn_toast, "Bitte Namen eingeben", C_WARN);
    return;
  }
  if (g_spool_count >= SPOOL_MAX) return;
  spool_t *p = &g_spools[g_spool_count++];
  strncpy(p->name, sn_name, sizeof(p->name) - 1);
  p->name[sizeof(p->name) - 1] = 0;
  p->grams = g;
  spools_save();
  sound_play(SND_SAVE);
  sn_name[0] = 0;
  s_spool = g_spool_count - 1;
  ui_switch_page(page_spool_pick_create());
}

static lv_obj_t *page_spool_new_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Leere Spule auflegen", &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -120);

  sn_weight = ui_label(s, "", &font_sg_34, C_TEXT);
  lv_obj_align(sn_weight, LV_ALIGN_CENTER, 0, -72);
  sn_state = ui_chip(s, "misst …", C_MUTED);
  lv_obj_align(sn_state, LV_ALIGN_CENTER, 0, -30);

  char b[40];
  snprintf(b, sizeof(b), T("Name: %s ›"), sn_name[0] ? sn_name : T("eingeben"));
  lv_obj_t *nb = ui_btn(s, b, BTN_NORMAL);
  lv_obj_set_height(nb, 44);
  lv_obj_set_style_text_font(lv_obj_get_child(nb, 0), &font_sg_14, 0);
  lv_obj_align(nb, LV_ALIGN_CENTER, 0, 24);
  lv_obj_add_event_cb(nb, sn_name_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *sv = ui_btn(s, "Speichern", BTN_PRIMARY);
  lv_obj_align(sv, LV_ALIGN_CENTER, 0, 86);
  lv_obj_add_event_cb(sv, sn_save_cb, LV_EVENT_CLICKED, NULL);

  sn_toast = ui_toast_create(s);
  lv_obj_align(sn_toast, LV_ALIGN_CENTER, 0, 140);

  ui_page_timer(s, sn_timer_cb, 100);
  sn_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  3. Ergebnis
// ------------------------------------------------------------
static lv_obj_t *rs_ring, *rs_len, *rs_net, *rs_diam, *rs_toast;

static void rs_timer_cb(lv_timer_t *t) {
  float net = scale_net() - spool_weight();
  if (net < 0) net = 0;
  char b[40], w[16];
  snprintf(b, sizeof(b), "%d", (int)lroundf(length_m(net)));
  ui_label_update(rs_len, b);
  ui_fmt_weight(w, sizeof(w), net);
  snprintf(b, sizeof(b), T("%s g Filament"), w);
  ui_label_update(rs_net, b);
  // Ergebnis automatisch mitschreiben, sobald die Spule ruhig liegt
  char note[40];
  snprintf(note, sizeof(note), T("Spule %s: %d m"), MATERIALS[s_mat].name, (int)lroundf(length_m(net)));
  if (track_update(net, scale_stable(), note)) ui_toast_show(rs_toast, "Im Protokoll gespeichert", C_ACCENT);

  int v = (int)(net / 1000.0f * 1000.0f);  // Ring: Anteil einer 1-kg-Rolle
  if (v > 1000) v = 1000;
  if (lv_arc_get_value(rs_ring) != v) lv_arc_set_value(rs_ring, v);
}

static void rs_diam_cb(lv_event_t *e) {
  s_diam = s_diam < 2.0f ? 2.85f : 1.75f;
  lv_label_set_text(lv_obj_get_child(rs_diam, 0), s_diam < 2.0f ? "1,75 mm" : "2,85 mm");
}

static void rs_other_cb(lv_event_t *e) {
  ui_switch_page(page_spule_create());
}


static lv_obj_t *page_result_create() {
  lv_obj_t *s = ui_screen_create();
  rs_ring = ui_ring(s, 396);

  char b[48];
  snprintf(b, sizeof(b), "%s · %s", MATERIALS[s_mat].name, s_spool >= 0 ? g_spools[s_spool].name : T("ohne Spule"));
  lv_obj_t *t = ui_label(s, b, &font_sg_14, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);

  rs_diam = ui_btn(s, s_diam < 2.0f ? "1,75 mm" : "2,85 mm", BTN_NORMAL);
  lv_obj_set_height(rs_diam, 36);
  lv_obj_set_style_pad_hor(rs_diam, 12, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(rs_diam, 0), &font_sg_14, 0);
  lv_obj_align(rs_diam, LV_ALIGN_TOP_MID, 0, 74);
  lv_obj_add_event_cb(rs_diam, rs_diam_cb, LV_EVENT_CLICKED, NULL);

  rs_len = ui_label(s, "0", &font_sg_80, C_TEXT);
  lv_obj_align(rs_len, LV_ALIGN_CENTER, 0, -8);
  lv_obj_t *m = ui_label(s, "m Restfilament", &font_sg_18, C_ACCENT);
  lv_obj_align(m, LV_ALIGN_CENTER, 0, 50);
  rs_net = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(rs_net, LV_ALIGN_CENTER, 0, 76);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 124);
  lv_obj_t *o = ui_btn(row, "Andere Spule", BTN_NORMAL);
  lv_obj_add_event_cb(o, rs_other_cb, LV_EVENT_CLICKED, NULL);

  rs_toast = ui_toast_create(s);
  lv_obj_align(rs_toast, LV_ALIGN_TOP_MID, 0, 118);

  track_reset();
  ui_page_timer(s, rs_timer_cb, 200);
  rs_timer_cb(NULL);
  return s;
}
