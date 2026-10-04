#include "ui_text.h"
#include "ui_theme.h"
#include "sound.h"
#include <string.h>
#include <strings.h>
#include <math.h>
#include <stdio.h>

#define RING_R 180      // Radius der Buchstaben
#define TOUCH_MIN_R 120 // innerhalb davon zählt die Berührung nicht als Ring
#define KEYS_MAX 34
#define TEXT_MAX 72

// Die letzten drei Einträge jedes Satzes: Leerzeichen, Löschen, Umschalten
static const char *const SET_UPPER[] = {
  "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X",
  "Y","Z","Ä","Ö","Ü","Leer","←","abc" };
static const char *const SET_LOWER[] = {
  "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x",
  "y","z","ä","ö","ü","ß","Leer","←","123" };
static const char *const SET_SYM[] = {
  "0","1","2","3","4","5","6","7","8","9",".",",","-","_","@","#","!","?","&","/","(",")","+","*",
  "=",":","%","Leer","←","ABC" };

static const char *const *const SETS[3] = { SET_UPPER, SET_LOWER, SET_SYM };
static const int SET_N[3] = { sizeof(SET_UPPER) / sizeof(char *), sizeof(SET_LOWER) / sizeof(char *),
                              sizeof(SET_SYM) / sizeof(char *) };

static char s_text[TEXT_MAX];
static int s_max;
static bool s_auto_shift;
static int s_set;
static int s_press = -1;
static ui_text_done_cb s_cb;
static const char *const *s_suggest;
static int s_n_suggest;
static const char *s_current_suggest;

static lv_obj_t *s_keys[KEYS_MAX];
static lv_obj_t *s_hl;
static lv_obj_t *s_center, *s_lbl_text, *s_btn_sugg;
static lv_obj_t *s_lupe, *s_lp_text, *s_lp_prev, *s_lp_cur, *s_lp_next;

static int n_keys() { return SET_N[s_set]; }
static bool is_special(int i) { return i >= n_keys() - 3; }

static void key_offset(int i, lv_coord_t *x, lv_coord_t *y) {
  float a = i * 6.2831853f / n_keys();
  *x = (lv_coord_t)(RING_R * sinf(a));
  *y = (lv_coord_t)(-RING_R * cosf(a));
}

static void layout_keys() {
  for (int i = 0; i < KEYS_MAX; i++) {
    if (i >= n_keys()) {
      lv_obj_add_flag(s_keys[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(s_keys[i], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_keys[i], SETS[s_set][i]);
    lv_obj_set_style_text_font(s_keys[i], is_special(i) ? &font_sg_14 : &font_sg_18, 0);
    lv_obj_set_style_text_color(s_keys[i], C_TEXT2, 0);
    lv_coord_t x, y;
    key_offset(i, &x, &y);
    lv_obj_align(s_keys[i], LV_ALIGN_CENTER, x, y);
  }
}

// Text für die Anzeige: nur das Ende, wenn er zu lang ist
static void tail(char *out, int len, int max_bytes) {
  const char *p = s_text;
  int l = strlen(p);
  if (l > max_bytes) {
    p = s_text + l - max_bytes;
    while ((*p & 0xC0) == 0x80) p++;  // nicht mitten in einem Umlaut beginnen
    snprintf(out, len, "…%s", p);
  } else {
    snprintf(out, len, "%s", p);
  }
}

static void refresh_text() {
  char t[TEXT_MAX + 8], b[TEXT_MAX + 8];
  tail(t, sizeof(t), 16);
  snprintf(b, sizeof(b), "%s|", t);
  lv_label_set_text(s_lbl_text, b);

  // Vorschlag: erster Eintrag, der mit dem bisherigen Text beginnt
  s_current_suggest = NULL;
  int l = strlen(s_text);
  if (l > 0) {
    for (int i = 0; i < s_n_suggest; i++) {
      const char *sg = T(s_suggest[i]);  // Vorschläge in der eingestellten Sprache
      if ((int)strlen(sg) > l && strncasecmp(sg, s_text, l) == 0) {
        s_current_suggest = sg;
        break;
      }
    }
  }
  if (s_current_suggest) {
    char sb[64];
    snprintf(sb, sizeof(sb), T("Vorschlag: %s"), s_current_suggest);
    lv_label_set_text(lv_obj_get_child(s_btn_sugg, 0), sb);
    lv_obj_clear_flag(s_btn_sugg, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_btn_sugg, LV_OBJ_FLAG_HIDDEN);
  }
}

static void backspace() {
  int l = strlen(s_text);
  if (l == 0) return;
  l--;
  while (l > 0 && (s_text[l] & 0xC0) == 0x80) l--;  // Umlaut = 2 Bytes
  s_text[l] = 0;
}

static void apply_key(int i) {
  int n = n_keys();
  if (i == n - 3) {  // Leer
    if ((int)strlen(s_text) + 1 < s_max) strcat(s_text, " ");
  } else if (i == n - 2) {  // Löschen
    backspace();
  } else if (i == n - 1) {  // Umschalten ABC -> abc -> 123 -> ABC
    s_set = (s_set + 1) % 3;
    layout_keys();
  } else {
    const char *k = SETS[s_set][i];
    bool first = s_text[0] == 0;
    if ((int)(strlen(s_text) + strlen(k)) < s_max) strcat(s_text, k);
    if (first && s_auto_shift && s_set == 0) {  // nach dem ersten Großbuchstaben klein weiter
      s_set = 1;
      layout_keys();
    }
  }
  refresh_text();
}

static int key_at(lv_point_t p) {
  float dx = p.x - SCREEN_SIZE / 2.0f, dy = p.y - SCREEN_SIZE / 2.0f;
  if (sqrtf(dx * dx + dy * dy) < TOUCH_MIN_R) return -1;
  float a = atan2f(dx, -dy);
  if (a < 0) a += 6.2831853f;
  int n = n_keys();
  return (int)lroundf(a / (6.2831853f / n)) % n;
}

static void show_press(int idx) {
  // alte Markierung zurücksetzen
  for (int i = 0; i < n_keys(); i++) lv_obj_set_style_text_color(s_keys[i], C_TEXT2, 0);

  if (idx < 0) {
    lv_obj_add_flag(s_hl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_lupe, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_center, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_coord_t x, y;
  key_offset(idx, &x, &y);
  lv_obj_align(s_hl, LV_ALIGN_CENTER, x, y);
  lv_obj_clear_flag(s_hl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_color(s_keys[idx], C_ON_PRIMARY, 0);

  int n = n_keys();
  lv_label_set_text(s_lp_prev, SETS[s_set][(idx + n - 1) % n]);
  lv_label_set_text(s_lp_cur, SETS[s_set][idx]);
  lv_label_set_text(s_lp_next, SETS[s_set][(idx + 1) % n]);
  lv_obj_set_style_text_font(s_lp_cur, is_special(idx) ? &font_sg_18 : &font_sg_34, 0);

  char t[TEXT_MAX + 8];
  tail(t, sizeof(t), 14);
  lv_label_set_text(s_lp_text, t[0] ? t : " ");

  lv_obj_add_flag(s_center, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s_lupe, LV_OBJ_FLAG_HIDDEN);
}

static void ring_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_point_t p;
  if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
    lv_indev_get_point(lv_indev_get_act(), &p);
    int idx = key_at(p);
    if (idx != s_press) {
      s_press = idx;
      show_press(idx);
    }
  } else if (code == LV_EVENT_RELEASED) {
    int idx = s_press;
    s_press = -1;
    show_press(-1);
    if (idx >= 0) {
      apply_key(idx);
      sound_play(SND_CLICK);
    }
  } else if (code == LV_EVENT_PRESS_LOST) {
    s_press = -1;
    show_press(-1);
  }
}

static void ok_cb(lv_event_t *e) {
  if (s_cb) s_cb(true, s_text);
}

static void cancel_cb(lv_event_t *e) {
  if (s_cb) s_cb(false, s_text);
}

static void sugg_cb(lv_event_t *e) {
  if (!s_current_suggest) return;
  strncpy(s_text, s_current_suggest, TEXT_MAX - 1);
  s_text[TEXT_MAX - 1] = 0;
  refresh_text();
}

lv_obj_t *ui_text_page_create(const char *title, const char *initial, int max_len, bool password,
                              const char *const *suggest, int n_suggest, ui_text_done_cb cb) {
  s_max = max_len < TEXT_MAX ? max_len : TEXT_MAX;
  strncpy(s_text, initial ? initial : "", TEXT_MAX - 1);
  s_text[TEXT_MAX - 1] = 0;
  s_auto_shift = !password && s_text[0] == 0;
  s_set = (password || s_text[0]) ? 1 : 0;
  s_cb = cb;
  s_suggest = suggest;
  s_n_suggest = suggest ? n_suggest : 0;
  s_press = -1;

  lv_obj_t *s = ui_screen_create();

  // Ring: ganze Fläche, nimmt die Berührungen am Rand entgegen
  lv_obj_t *ring = ui_box(s);
  lv_obj_set_size(ring, SCREEN_SIZE, SCREEN_SIZE);
  lv_obj_add_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ring, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_GESTURE_BUBBLE);  // Wischen am Rand = Tippen, nicht "zurück"
  lv_obj_add_event_cb(ring, ring_cb, LV_EVENT_ALL, NULL);

  s_hl = ui_box(ring);
  lv_obj_set_size(s_hl, 40, 40);
  lv_obj_set_style_radius(s_hl, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_hl, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_hl, C_PRIMARY, 0);
  lv_obj_add_flag(s_hl, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < KEYS_MAX; i++) s_keys[i] = ui_label(ring, "", &font_sg_18, C_TEXT2);
  layout_keys();

  // Mitte im Ruhezustand
  s_center = ui_box(s);
  lv_obj_set_size(s_center, 250, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s_center, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_center, 10, 0);
  lv_obj_center(s_center);

  ui_label(s_center, title, &font_sg_14, C_MUTED);
  s_lbl_text = ui_label(s_center, "", &font_sg_24, C_TEXT);

  s_btn_sugg = ui_btn(s_center, "", BTN_NORMAL);
  lv_obj_set_height(s_btn_sugg, 40);
  lv_obj_set_style_pad_hor(s_btn_sugg, 14, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(s_btn_sugg, 0), &font_sg_14, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btn_sugg, 0), C_ACCENT, 0);
  lv_obj_add_event_cb(s_btn_sugg, sugg_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *row = ui_box(s_center);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_t *b_cancel = ui_btn(row, "Abbrechen", BTN_NORMAL);
  lv_obj_set_height(b_cancel, 44);
  lv_obj_set_style_pad_hor(b_cancel, 16, 0);
  lv_obj_add_event_cb(b_cancel, cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *b_ok = ui_btn(row, "OK", BTN_PRIMARY);
  lv_obj_set_height(b_ok, 44);
  lv_obj_add_event_cb(b_ok, ok_cb, LV_EVENT_CLICKED, NULL);

  // Lupe beim Wischen
  s_lupe = ui_box(s);
  lv_obj_set_size(s_lupe, 250, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s_lupe, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_lupe, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_lupe, 12, 0);
  lv_obj_center(s_lupe);
  lv_obj_add_flag(s_lupe, LV_OBJ_FLAG_HIDDEN);

  s_lp_text = ui_label(s_lupe, "", &font_sg_24, C_MUTED);
  lv_obj_t *lrow = ui_box(s_lupe);
  lv_obj_set_size(lrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(lrow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(lrow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(lrow, 14, 0);
  s_lp_prev = ui_label(lrow, "", &font_sg_24, C_FAINT);
  lv_obj_t *circle = ui_box(lrow);
  lv_obj_set_size(circle, 92, 92);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(circle, C_PRIMARY, 0);
  s_lp_cur = ui_label(circle, "", &font_sg_34, C_ON_PRIMARY);
  lv_obj_center(s_lp_cur);
  s_lp_next = ui_label(lrow, "", &font_sg_24, C_FAINT);
  ui_label(s_lupe, "Loslassen übernimmt", &font_sg_14, C_MUTED);

  refresh_text();
  return s;
}
