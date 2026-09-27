#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "config.h"
#include "settings.h"
#include "sound.h"
#include <stdio.h>

// ------------------------------------------------------------
//  Wasserwaage
// ------------------------------------------------------------
static lv_obj_t *lvl_bubble, *lvl_angle, *lvl_xy;
static lv_timer_t *lvl_timer;

static void level_timer_cb(lv_timer_t *t) {
  float tilt, dx, dy;
  if (!ui_read_tilt(&tilt, &dx, &dy)) {
    ui_label_update(lvl_angle, "keine Lagedaten");
    return;
  }
  bool ok = tilt < LEVEL_OK_DEG;
  lv_color_t col = ok ? C_ACCENT : C_WARN;
  ui_bubble_set(lvl_bubble, dx, dy, col);

  char a[12], x[12], y[12], buf[48];
  ui_fmt_1dec(a, sizeof(a), tilt);
  snprintf(buf, sizeof(buf), "%s° · %s", a, ok ? T("ok") : T("schief"));
  ui_label_update(lvl_angle, buf);
  lv_obj_set_style_text_color(lvl_angle, col, 0);

  ui_fmt_1dec(x, sizeof(x), dx < 0 ? -dx : dx);
  ui_fmt_1dec(y, sizeof(y), dy < 0 ? -dy : dy);
  snprintf(buf, sizeof(buf), "X %s° · Y %s°", x, y);
  ui_label_update(lvl_xy, buf);
}

lv_obj_t *page_level_create() {
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *title = ui_label(s, "Wasserwaage", &font_sg_18, C_MUTED);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 44);

  lvl_bubble = ui_bubble_create(s, 210);
  lv_obj_align(lvl_bubble, LV_ALIGN_CENTER, 0, -14);

  lvl_angle = ui_label(s, "", &font_sg_24, C_ACCENT);
  lv_obj_align(lvl_angle, LV_ALIGN_CENTER, 0, 118);

  lvl_xy = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(lvl_xy, LV_ALIGN_CENTER, 0, 150);

  lvl_timer = ui_page_timer(s, level_timer_cb, 50);
  level_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Platzhalter
// ------------------------------------------------------------
static void back_btn_cb(lv_event_t *e) {
  ui_go_home();
}

lv_obj_t *page_placeholder_create(const char *title) {
  lv_obj_t *s = ui_screen_create();

  lv_obj_t *t = ui_label(s, title, &font_sg_34, C_TEXT);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -70);

  lv_obj_t *info = ui_label(s, "Diese Seite folgt\nim nächsten Schritt.", &font_sg_18, C_MUTED);
  lv_obj_align(info, LV_ALIGN_CENTER, 0, -8);

  lv_obj_t *hint = ui_label(s, "Nach rechts wischen: zurück", &font_sg_14, C_FAINT);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 48);

  lv_obj_t *b = ui_btn(s, "Zurück", BTN_NORMAL);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 112);
  lv_obj_add_event_cb(b, back_btn_cb, LV_EVENT_CLICKED, NULL);
  return s;
}

// ------------------------------------------------------------
//  Setup -> Libelle kalibrieren (Umschlagmethode)
//  Messung 1, Waage um 180° drehen, Messung 2.
//  Mittelwert beider Messungen = Einbaufehler des Displays,
//  die Schräge der Unterlage hebt sich dabei heraus.
// ------------------------------------------------------------
static lv_obj_t *lc_title, *lc_bubble, *lc_text, *lc_info, *lc_btn_measure, *lc_btn_reset, *lc_btn_done;
static int lc_step = 0;
static float lc_x = 0, lc_y = 0;   // geglättete Rohwerte
static float lc_x1 = 0, lc_y1 = 0; // erste Messung

static void lc_show() {
  char b[64];
  snprintf(b, sizeof(b), T("Libelle kalibrieren · %d / 2"), lc_step < 2 ? lc_step + 1 : 2);
  lv_label_set_text(lc_title, b);
  if (lc_step == 0) lv_label_set_text(lc_text, "Waage abstellen, dann Messen");
  else if (lc_step == 1) lv_label_set_text(lc_text, "Waage um 180° drehen, dann Messen");
  else lv_label_set_text(lc_text, "Nullpunkt gespeichert");
  lv_obj_set_style_text_color(lc_text, lc_step == 2 ? C_ACCENT : C_TEXT, 0);
  if (lc_step == 2) {
    lv_obj_add_flag(lc_btn_measure, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lc_btn_reset, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lc_btn_done, LV_OBJ_FLAG_HIDDEN);
  }
}

static void lc_timer_cb(lv_timer_t *t) {
  float dx, dy;
  if (!ui_read_tilt_raw(&dx, &dy)) return;
  lc_x += (dx - lc_x) * 0.2f;  // glätten
  lc_y += (dy - lc_y) * 0.2f;
  ui_bubble_set(lc_bubble, lc_x - (lc_step == 2 ? g_set.lvl_off_x : 0),
                lc_y - (lc_step == 2 ? g_set.lvl_off_y : 0), lc_step == 2 ? C_ACCENT : C_WARN);
}

static void lc_measure_cb(lv_event_t *e) {
  if (lc_step == 0) {
    lc_x1 = lc_x;
    lc_y1 = lc_y;
    lc_step = 1;
    sound_play(SND_TARA);
  } else if (lc_step == 1) {
    g_set.lvl_off_x = (lc_x1 + lc_x) / 2.0f;
    g_set.lvl_off_y = (lc_y1 + lc_y) / 2.0f;
    settings_save();
    lc_step = 2;
    char a[12], b[12], c[48];
    ui_fmt_1dec(a, sizeof(a), g_set.lvl_off_x);
    ui_fmt_1dec(b, sizeof(b), g_set.lvl_off_y);
    snprintf(c, sizeof(c), T("Versatz X %s° · Y %s°"), a, b);
    lv_label_set_text(lc_info, c);
    sound_play(SND_DONE);
  }
  lc_show();
}

static void lc_reset_cb(lv_event_t *e) {
  g_set.lvl_off_x = 0;
  g_set.lvl_off_y = 0;
  settings_save();
  lv_label_set_text(lc_info, "Nullpunkt zurückgesetzt");
  sound_play(SND_TARA);
}

static void lc_done_cb(lv_event_t *e) {
  ui_switch_page(page_setup_back());
}

lv_obj_t *page_level_setup_create() {
  lv_obj_t *s = ui_screen_create();
  lc_step = 0;
  ui_read_tilt_raw(&lc_x, &lc_y);

  lc_title = ui_label(s, "", &font_sg_18, C_MUTED);
  lv_obj_align(lc_title, LV_ALIGN_TOP_MID, 0, 46);

  lc_bubble = ui_bubble_create(s, 140);
  lv_obj_align(lc_bubble, LV_ALIGN_CENTER, 0, -46);

  lc_text = ui_label(s, "", &font_sg_18, C_TEXT);
  lv_obj_align(lc_text, LV_ALIGN_CENTER, 0, 44);
  lc_info = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(lc_info, LV_ALIGN_CENTER, 0, 72);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 120);
  lc_btn_reset = ui_btn(row, "Zurücksetzen", BTN_NORMAL);
  lv_obj_set_style_pad_hor(lc_btn_reset, 16, 0);
  lv_obj_add_event_cb(lc_btn_reset, lc_reset_cb, LV_EVENT_CLICKED, NULL);
  lc_btn_measure = ui_btn(row, "Messen", BTN_PRIMARY);
  lv_obj_add_event_cb(lc_btn_measure, lc_measure_cb, LV_EVENT_CLICKED, NULL);
  lc_btn_done = ui_btn(row, "Fertig", BTN_PRIMARY);
  lv_obj_add_event_cb(lc_btn_done, lc_done_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(lc_btn_done, LV_OBJ_FLAG_HIDDEN);

  ui_page_timer(s, lc_timer_cb, 50);
  lc_show();
  return s;
}
