#include "tools.h"
#include "ui_theme.h"
#include "ui_power.h"
#include "hal.h"
#include "scale.h"
#include "sound.h"
#include "storage.h"
#include "data.h"
#include "update_online.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================
//  Küchentimer
// ============================================================
typedef struct {
  bool running;
  bool ringing;
  uint32_t end_ms;
  uint32_t dur_s;
  uint32_t ring_since;
  char name[20];
} kt_t;

static kt_t s_kt[TIMER_COUNT];
static lv_obj_t *a_box, *a_name, *a_info;  // Alarm-Überlagerung (lazy)
static uint32_t a_last_sound = 0;

#define RING_MAX_MS 120000  // nach 2 min von selbst still
#define RING_EVERY_MS 1600

void timer_fmt(char *b, int len, int secs) {
  if (secs < 0) secs = 0;
  if (secs >= 3600) snprintf(b, len, "%d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
  else snprintf(b, len, "%d:%02d", secs / 60, secs % 60);
}

void timer_start(int i, uint32_t secs, const char *name) {
  if (i < 0 || i >= TIMER_COUNT || secs == 0) return;
  kt_t &t = s_kt[i];
  t.running = true;
  t.ringing = false;
  t.dur_s = secs;
  t.end_ms = hal_millis() + secs * 1000UL;
  if (name && name[0]) snprintf(t.name, sizeof(t.name), "%s", name);
  else t.name[0] = 0;
}

void timer_stop(int i) {
  if (i < 0 || i >= TIMER_COUNT) return;
  s_kt[i].running = false;
  s_kt[i].ringing = false;
}

void timer_add(int i, int secs) {
  if (i < 0 || i >= TIMER_COUNT || !s_kt[i].running) return;
  int left = timer_left(i) + secs;
  if (left < 1) left = 1;
  s_kt[i].end_ms = hal_millis() + (uint32_t)left * 1000UL;
}

bool timer_running(int i) {
  return i >= 0 && i < TIMER_COUNT && s_kt[i].running;
}

bool timer_ringing(int i) {
  return timer_running(i) && s_kt[i].ringing;
}

int timer_left(int i) {
  if (!timer_running(i)) return -1;
  int32_t d = (int32_t)(s_kt[i].end_ms - hal_millis());
  return d <= 0 ? 0 : (d + 999) / 1000;
}

uint32_t timer_duration(int i) {
  return (i >= 0 && i < TIMER_COUNT) ? s_kt[i].dur_s : 0;
}

const char *timer_name(int i) {
  static char b[24];
  if (i >= 0 && i < TIMER_COUNT && s_kt[i].name[0]) return s_kt[i].name;
  snprintf(b, sizeof(b), T("Timer %d"), i + 1);
  return b;
}

int timer_next() {
  int best = -1, bl = 0;
  for (int i = 0; i < TIMER_COUNT; i++) {
    if (!s_kt[i].running || s_kt[i].ringing) continue;
    int l = timer_left(i);
    if (best < 0 || l < bl) {
      best = i;
      bl = l;
    }
  }
  return best;
}

int timer_free() {
  for (int i = 0; i < TIMER_COUNT; i++)
    if (!s_kt[i].running) return i;
  return -1;
}

static void alarm_off() {
  for (int i = 0; i < TIMER_COUNT; i++)
    if (s_kt[i].ringing) timer_stop(i);
  if (a_box) lv_obj_add_flag(a_box, LV_OBJ_FLAG_HIDDEN);
}

static void alarm_show(int i) {
  if (!a_box) {
    a_box = ui_box(lv_layer_top());
    lv_obj_set_size(a_box, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(a_box, C_BG, 0);
    lv_obj_set_style_bg_opa(a_box, LV_OPA_COVER, 0);
    lv_obj_add_flag(a_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(a_box, [](lv_event_t *e) { alarm_off(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *r = ui_ring(a_box, 396);
    lv_arc_set_value(r, 1000);
    lv_obj_set_style_arc_color(r, C_WARN, LV_PART_INDICATOR);
    lv_obj_t *ic = ui_label(a_box, ICON_TIMER, &font_icons_26, C_WARN);
    lv_obj_align(ic, LV_ALIGN_CENTER, 0, -86);
    a_name = ui_label(a_box, "", &font_sg_34, C_TEXT);
    lv_obj_align(a_name, LV_ALIGN_CENTER, 0, -36);
    a_info = ui_label(a_box, "", &font_sg_24, C_WARN);
    lv_obj_align(a_info, LV_ALIGN_CENTER, 0, 12);
    lv_obj_t *h = ui_label(a_box, "Antippen zum Beenden", &font_sg_14, C_MUTED);
    lv_obj_align(h, LV_ALIGN_CENTER, 0, 70);
  }
  lv_label_set_text(a_name, timer_name(i));
  lv_label_set_text(a_info, "abgelaufen");
  lv_obj_clear_flag(a_box, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(a_box);
  ui_power_wake();  // aus dem Standby holen
}

static void timers_tick() {
  uint32_t now = hal_millis();
  bool any_ring = false;
  for (int i = 0; i < TIMER_COUNT; i++) {
    kt_t &t = s_kt[i];
    if (!t.running) continue;
    if (!t.ringing && (int32_t)(t.end_ms - now) <= 0) {
      t.ringing = true;
      t.ring_since = now;
      a_last_sound = 0;
      alarm_show(i);
      char note[32];
      snprintf(note, sizeof(note), T("%s abgelaufen"), timer_name(i));
      log_add(scale_net(), note);
    }
    if (t.ringing) {
      if (now - t.ring_since > RING_MAX_MS) timer_stop(i);
      else any_ring = true;
    }
  }
  if (any_ring) {
    if (!a_last_sound || now - a_last_sound > RING_EVERY_MS) {
      a_last_sound = now;
      sound_play(SND_DONE);
    }
  } else if (a_box && !lv_obj_has_flag(a_box, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(a_box, LV_OBJ_FLAG_HIDDEN);
  }
}

// ============================================================
//  Langzeitmessung
// ============================================================
#define DIR_LONG "/Waage/Messung"
static bool s_lt = false;
static int s_lt_iv = 60;
static uint32_t s_lt_t0 = 0, s_lt_last = 0;
static int s_lt_n = 0;
static float s_lt_first = 0;
static char s_lt_file[40];
static float s_lt_h[LT_HIST];
static uint32_t s_lt_ht[LT_HIST];  // Zeitpunkt (s seit Start) je Punkt
static int s_lt_hn = 0, s_lt_hp = 0;

bool lt_start(int interval_s) {
  if (!storage_ok()) return false;
  storage_mkdir(DIR_LONG);
  int y, mo, d, h, mi, s;
  if (hal_now(&y, &mo, &d, &h, &mi, &s)) snprintf(s_lt_file, sizeof(s_lt_file), "%04d-%02d-%02d_%02d%02d.csv", y, mo, d, h, mi);
  else snprintf(s_lt_file, sizeof(s_lt_file), "messung_%lu.csv", (unsigned long)(hal_millis() / 1000));
  char path[80];
  snprintf(path, sizeof(path), DIR_LONG "/%s", s_lt_file);
  if (!storage_write(path, "Uhrzeit;Minuten;Gewicht_g\n")) return false;
  s_lt = true;
  s_lt_iv = interval_s < 1 ? 1 : interval_s;
  s_lt_t0 = hal_millis();
  s_lt_last = 0;
  s_lt_n = 0;
  s_lt_hn = s_lt_hp = 0;
  return true;
}

void lt_stop() {
  if (!s_lt) return;
  s_lt = false;
  char note[48];
  snprintf(note, sizeof(note), T("Langzeitmessung: %d Werte"), s_lt_n);
  log_add(scale_net(), note);
}

bool lt_running() { return s_lt; }
int lt_interval() { return s_lt_iv; }
int lt_samples() { return s_lt_n; }
float lt_first() { return s_lt_first; }
const char *lt_file() { return s_lt_file; }

float lt_hours() {
  return s_lt ? (hal_millis() - s_lt_t0) / 3600000.0f : 0;
}

int lt_hist(float *out, int max) {
  int n = s_lt_hn < max ? s_lt_hn : max;
  for (int k = 0; k < n; k++) out[k] = s_lt_h[(s_lt_hp - n + k + LT_HIST) % LT_HIST];
  return n;
}

// Steigung (lineare Regression) über die letzten bis zu 30 Punkte, in g pro Stunde
float lt_trend() {
  int n = s_lt_hn < 30 ? s_lt_hn : 30;
  if (n < 3) return 0;
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (int k = 0; k < n; k++) {
    int i = (s_lt_hp - n + k + LT_HIST) % LT_HIST;
    double x = s_lt_ht[i] / 3600.0, y = s_lt_h[i];
    sx += x; sy += y; sxx += x * x; sxy += x * y;
  }
  double den = n * sxx - sx * sx;
  return den > 1e-12 ? (float)((n * sxy - sx * sy) / den) : 0;
}

static void lt_tick() {
  if (!s_lt) return;
  uint32_t now = hal_millis();
  if (s_lt_last && now - s_lt_last < (uint32_t)s_lt_iv * 1000UL) return;
  s_lt_last = now;
  float g = scale_net();
  uint32_t secs = (now - s_lt_t0) / 1000;
  if (s_lt_n == 0) s_lt_first = g;
  char line[64], w[16], clock[12] = "--:--:--";
  int y, mo, d, h, mi, s;
  if (hal_now(&y, &mo, &d, &h, &mi, &s)) snprintf(clock, sizeof(clock), "%02d:%02d:%02d", h, mi, s);
  fmt_num(w, sizeof(w), g, 2);
  snprintf(line, sizeof(line), "%s;%.2f;%s", clock, secs / 60.0f, w);
  for (char *c = line; *c; c++)  // Minuten ebenfalls mit Komma (Excel)
    if (*c == '.') *c = ',';
  char path[80];
  snprintf(path, sizeof(path), DIR_LONG "/%s", s_lt_file);
  storage_append(path, line);
  s_lt_h[s_lt_hp] = g;
  s_lt_ht[s_lt_hp] = secs;
  s_lt_hp = (s_lt_hp + 1) % LT_HIST;
  if (s_lt_hn < LT_HIST) s_lt_hn++;
  s_lt_n++;
}

// ============================================================
void tools_tick() {
  timers_tick();
  lt_tick();
}

bool tools_keep_awake() {
  if (s_lt || upd_busy()) return true;
  for (int i = 0; i < TIMER_COUNT; i++)
    if (s_kt[i].running) return true;
  return false;
}
