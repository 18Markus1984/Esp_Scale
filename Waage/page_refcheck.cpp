// ============================================================
//  Prüfgewicht (Setup -> Waage -> Prüfgewicht)
//  Einstellen: Sollgewicht, Intervall, Toleranz. Prüfen: leeren,
//  Prüfgewicht auflegen, ruhig liegen lassen -> Ergebnis wird
//  gespeichert (Flash + /Waage/Pruefung/pruefgewicht.csv + Protokoll).
//  Ist die Prüfung fällig, erinnert die Waage nach dem Start.
// ============================================================
#include "ui_pages.h"
#include "ui.h"
#include "ui_theme.h"
#include "scale.h"
#include "sound.h"
#include "settings.h"
#include "data.h"
#include "refcheck.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

static const float REF_PRESETS[] = { 0, 50, 100, 200, 500, 1000, 2000 };
static const int DAY_STEPS[] = { 0, 7, 14, 30, 60, 90, 180 };
static const float TOL_STEPS[] = { 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f };
#define NUM(a) (int)(sizeof(a) / sizeof(a[0]))

// ------------------------------------------------------------
//  Einstellungen
// ------------------------------------------------------------
static lv_obj_t *rs_val[3], *rs_info, *rs_go;

static void rs_show() {
  char b[64], w[16];
  if (g_set.ref_g <= 0) snprintf(b, sizeof(b), "%s", T("aus ›"));
  else {
    fmt_num(w, sizeof(w), g_set.ref_g, g_set.ref_g < 100 ? 1 : 0);
    snprintf(b, sizeof(b), "%s g ›", w);
  }
  lv_label_set_text(rs_val[0], b);
  if (g_set.ref_days <= 0) snprintf(b, sizeof(b), "%s", T("nie ›"));
  else snprintf(b, sizeof(b), T("%d Tage ›"), g_set.ref_days);
  lv_label_set_text(rs_val[1], b);
  fmt_num(w, sizeof(w), g_set.ref_tol, 1);
  snprintf(b, sizeof(b), "±%s g ›", w);
  lv_label_set_text(rs_val[2], b);

  // letzte Prüfung und nächster Termin
  char d[16], m[16], dv[16], line[120];
  if (g_set.ref_last > 0) {
    refchk_date(g_set.ref_last, d, sizeof(d));
    fmt_num(m, sizeof(m), g_set.ref_meas, 2);
    float dev = g_set.ref_meas - g_set.ref_g;
    fmt_num(dv, sizeof(dv), fabsf(dev), 2);
    snprintf(line, sizeof(line), T("Zuletzt %s: %s g (%s%s g) %s"), d, m, dev < 0 ? "−" : "+", dv,
             g_set.ref_ok ? "OK" : T("außer Toleranz"));
  } else {
    snprintf(line, sizeof(line), "%s", T("Noch nicht geprüft"));
  }
  int dl = refchk_days_left();
  if (g_set.ref_g > 0 && g_set.ref_days > 0 && g_set.ref_last > 0 && dl < 9999) {
    char nd[16];
    refchk_date(g_set.ref_last + g_set.ref_days, nd, sizeof(nd));
    char tail[48];
    if (dl <= 0) snprintf(tail, sizeof(tail), "\n%s", T("Prüfung fällig"));
    else snprintf(tail, sizeof(tail), T("\nNächste Prüfung %s"), nd);
    strncat(line, tail, sizeof(line) - strlen(line) - 1);
  }
  lv_label_set_text(rs_info, line);
  lv_obj_set_style_text_color(rs_info, g_set.ref_last > 0 && !g_set.ref_ok ? C_WARN : C_MUTED, 0);
  if (g_set.ref_g > 0) lv_obj_clear_state(rs_go, LV_STATE_DISABLED);
  else lv_obj_add_state(rs_go, LV_STATE_DISABLED);
}

static void rs_row_cb(lv_event_t *e) {
  int r = (int)(intptr_t)lv_event_get_user_data(e);
  if (r == 0) {
    int k = 0;
    while (k < NUM(REF_PRESETS) && REF_PRESETS[k] <= g_set.ref_g + 0.01f) k++;
    g_set.ref_g = k < NUM(REF_PRESETS) ? REF_PRESETS[k] : 0;  // nächster Wert, danach "aus"
  } else if (r == 1) {
    int k = 0;
    while (k < NUM(DAY_STEPS) && DAY_STEPS[k] != g_set.ref_days) k++;
    g_set.ref_days = DAY_STEPS[(k + 1) % NUM(DAY_STEPS)];
  } else {
    int k = 0;
    while (k < NUM(TOL_STEPS) && fabsf(TOL_STEPS[k] - g_set.ref_tol) > 0.01f) k++;
    g_set.ref_tol = TOL_STEPS[(k + 1) % NUM(TOL_STEPS)];
  }
  settings_save();
  sound_play(SND_CLICK);
  rs_show();
}

lv_obj_t *page_refset_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *t = ui_label(s, "Prüfgewicht", &font_sg_24, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 44);
  static const char *const NAMES[3] = { "Gewicht", "Erinnern alle", "Toleranz" };
  for (int r = 0; r < 3; r++) {
    lv_obj_t *row = ui_box(s);
    lv_obj_set_size(row, 280, 44);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 88 + r * 46);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, C_TRACK, 0);
    lv_obj_set_style_bg_color(row, C_SURFACE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, rs_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)r);
    lv_obj_t *l = ui_label(row, NAMES[r], &font_sg_18, C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
    rs_val[r] = ui_label(row, "", &font_sg_18, C_ACCENT);
    lv_obj_align(rs_val[r], LV_ALIGN_RIGHT_MID, -4, 0);
  }
  rs_info = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_set_width(rs_info, 300);
  lv_label_set_long_mode(rs_info, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(rs_info, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(rs_info, LV_ALIGN_TOP_MID, 0, 234);

  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -36);
  lv_obj_t *back = ui_btn(row, "Zurück", BTN_NORMAL);
  lv_obj_set_height(back, 46);
  lv_obj_add_event_cb(back, [](lv_event_t *e) { ui_switch_page(page_setup_cat_create(3)); }, LV_EVENT_CLICKED, NULL);
  rs_go = ui_btn(row, "Prüfen", BTN_PRIMARY);
  lv_obj_set_height(rs_go, 46);
  lv_obj_add_event_cb(rs_go, [](lv_event_t *e) {
    if (g_set.ref_g > 0) ui_switch_page(page_refcheck_create());
  }, LV_EVENT_CLICKED, NULL);
  rs_show();
  return s;
}

// ------------------------------------------------------------
//  Prüfung: 0 = leeren, 1 = auflegen, 2 = Ergebnis
// ------------------------------------------------------------
static lv_obj_t *rc_ring, *rc_big, *rc_state, *rc_sub, *rc_row;
static int rc_st;
static uint32_t rc_since;
static float rc_sum;
static int rc_n;

static void rc_result(float meas) {
  bool ok = refchk_store(meas);
  rc_st = 2;
  char b[48], w[16];
  fmt_num(w, sizeof(w), meas, 2);
  lv_label_set_text(rc_big, w);  // große Schrift hat nur Ziffern
  lv_obj_set_style_text_color(rc_big, ok ? C_ACCENT : C_WARN, 0);
  float dev = meas - g_set.ref_g;
  fmt_num(w, sizeof(w), fabsf(dev), 2);
  snprintf(b, sizeof(b), T("Abweichung %s%s g"), dev < 0 ? "−" : "+", w);
  lv_label_set_text(rc_state, b);
  lv_label_set_text(rc_sub, ok ? T("In Ordnung · gespeichert") : T("Außer Toleranz · bitte kalibrieren"));
  lv_obj_set_style_text_color(rc_sub, ok ? C_ACCENT : C_WARN, 0);
  lv_arc_set_value(rc_ring, 1000);
  lv_obj_set_style_arc_color(rc_ring, ok ? C_ACCENT : C_WARN, LV_PART_INDICATOR);
  // Knöpfe: Fertig, bei Fehler zusätzlich Kalibrieren
  lv_obj_clean(rc_row);
  lv_obj_t *fin = ui_btn(rc_row, "Fertig", ok ? BTN_PRIMARY : BTN_NORMAL);
  lv_obj_add_event_cb(fin, [](lv_event_t *e) { ui_switch_page(page_refset_create()); }, LV_EVENT_CLICKED, NULL);
  if (!ok) {
    lv_obj_t *cal = ui_btn(rc_row, "Kalibrieren", BTN_WARN);
    lv_obj_add_event_cb(cal, [](lv_event_t *e) { ui_switch_page(page_kalib_create()); }, LV_EVENT_CLICKED, NULL);
  }
  sound_play(ok ? SND_DONE : SND_WARN);
}

static void rc_timer_cb(lv_timer_t *t) {
  if (rc_st == 2) return;
  float g = scale_net();
  uint32_t now = lv_tick_get();
  char b[40], w[16];
  if (rc_st == 0) {
    lv_label_set_text(rc_big, "-");
    lv_label_set_text(rc_state, T("Waage leeren"));
    if (scale_stable() && fabsf(g) < 2.0f) {
      if (!rc_since) rc_since = now;
      if (now - rc_since > 1000) {
        scale_tare();
        sound_play(SND_TARA);
        rc_st = 1;
        rc_since = 0;
      }
    } else {
      rc_since = 0;
    }
    return;
  }
  // Prüfgewicht liegt: 2 s ruhig mitteln, dann übernehmen
  fmt_num(w, sizeof(w), g, 2);
  lv_label_set_text(rc_big, w);
  int v = (int)(g / g_set.ref_g * 1000);
  lv_arc_set_value(rc_ring, v < 0 ? 0 : (v > 1000 ? 1000 : v));
  fmt_num(w, sizeof(w), g_set.ref_g, g_set.ref_g < 100 ? 1 : 0);
  if (g < g_set.ref_g * 0.5f) {
    snprintf(b, sizeof(b), T("Prüfgewicht %s g auflegen"), w);
    lv_label_set_text(rc_state, b);
    rc_since = 0;
    return;
  }
  // Mittelung starten, sobald die Waage ruhig ist; nur neu beginnen, wenn sich
  // das Gewicht wirklich ändert (kurzes Flackern der Stabilität stört nicht)
  if (!rc_since) {
    if (!scale_stable()) {
      lv_label_set_text(rc_state, T("misst …"));
      return;
    }
    rc_since = now;
    rc_sum = 0;
    rc_n = 0;
  } else if (rc_n > 0 && fabsf(g - rc_sum / rc_n) > 0.3f) {
    rc_since = 0;
    lv_label_set_text(rc_state, T("misst …"));
    return;
  }
  lv_label_set_text(rc_state, T("ruhig – wird übernommen"));
  rc_sum += g;
  rc_n++;
  if (now - rc_since > 2000) rc_result(rc_sum / rc_n);
}

lv_obj_t *page_refcheck_create() {
  lv_obj_t *s = ui_screen_create();
  rc_ring = ui_ring(s, 396);
  lv_arc_set_value(rc_ring, 0);
  char b[40], w[16];
  fmt_num(w, sizeof(w), g_set.ref_g, g_set.ref_g < 100 ? 1 : 0);
  snprintf(b, sizeof(b), T("Prüfgewicht %s g"), w);
  lv_obj_t *t = ui_label(s, b, &font_sg_18, C_MUTED);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 64);
  rc_big = ui_label(s, "-", &font_sg_80, C_TEXT);
  lv_obj_align(rc_big, LV_ALIGN_CENTER, 0, -34);
  rc_state = ui_label(s, "", &font_sg_18, C_TEXT2);
  lv_obj_align(rc_state, LV_ALIGN_CENTER, 0, 30);
  rc_sub = ui_label(s, "", &font_sg_14, C_MUTED);
  lv_obj_align(rc_sub, LV_ALIGN_CENTER, 0, 58);
  fmt_num(w, sizeof(w), g_set.ref_tol, 1);
  snprintf(b, sizeof(b), T("Toleranz ±%s g"), w);
  lv_label_set_text(rc_sub, b);
  rc_row = ui_box(s);
  lv_obj_set_size(rc_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rc_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(rc_row, 10, 0);
  lv_obj_align(rc_row, LV_ALIGN_CENTER, 0, 118);
  lv_obj_t *tara = ui_btn(rc_row, "Tara", BTN_NORMAL);
  lv_obj_add_event_cb(tara, [](lv_event_t *e) {
    scale_tare();
    sound_play(SND_TARA);
    if (rc_st == 0) rc_st = 1;
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *cancel = ui_btn(rc_row, "Abbrechen", BTN_NORMAL);
  lv_obj_add_event_cb(cancel, [](lv_event_t *e) { ui_switch_page(page_refset_create()); }, LV_EVENT_CLICKED, NULL);
  rc_st = 0;
  rc_since = 0;
  ui_page_timer(s, rc_timer_cb, 100);
  rc_timer_cb(NULL);
  return s;
}

// ------------------------------------------------------------
//  Erinnerung nach dem Start
// ------------------------------------------------------------
lv_obj_t *page_refdue_create() {
  lv_obj_t *s = ui_screen_create();
  lv_obj_t *ic = ui_label(s, "Prüfgewicht", &font_sg_18, C_MUTED);
  lv_obj_align(ic, LV_ALIGN_CENTER, 0, -96);
  lv_obj_t *t = ui_label(s, "Prüfung fällig", &font_sg_34, C_WARN);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -54);
  char b[80], w[16], d[16];
  fmt_num(w, sizeof(w), g_set.ref_g, g_set.ref_g < 100 ? 1 : 0);
  if (g_set.ref_last > 0) {
    refchk_date(g_set.ref_last, d, sizeof(d));
    snprintf(b, sizeof(b), T("%s g auflegen · zuletzt %s"), w, d);
  } else {
    snprintf(b, sizeof(b), T("%s g auflegen · noch nie geprüft"), w);
  }
  lv_obj_t *sub = ui_label(s, b, &font_sg_14, C_TEXT2);
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, -12);
  lv_obj_t *row = ui_box(s);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 60);
  lv_obj_t *go = ui_btn(row, "Jetzt prüfen", BTN_PRIMARY);
  lv_obj_add_event_cb(go, [](lv_event_t *e) { ui_switch_page(page_refcheck_create()); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *later = ui_btn(row, "Später", BTN_NORMAL);
  lv_obj_add_event_cb(later, [](lv_event_t *e) { ui_go_home(); }, LV_EVENT_CLICKED, NULL);
  return s;
}
