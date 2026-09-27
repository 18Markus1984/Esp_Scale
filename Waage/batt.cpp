#include "batt.h"
#include "hal.h"
#include "storage.h"
#include "data.h"
#include <stdio.h>
#include <string.h>

static uint8_t s_pct[BATT_POINTS];
static int s_count = 0;
static uint32_t s_last = 0;

static void push(int pct) {
  if (s_count < BATT_POINTS) {
    s_pct[s_count++] = (uint8_t)pct;
    return;
  }
  memmove(s_pct, s_pct + 1, BATT_POINTS - 1);
  s_pct[BATT_POINTS - 1] = (uint8_t)pct;
}

// ------------------------------------------------------------
//  Entladetest
// ------------------------------------------------------------
static bool s_test = false;
static uint32_t s_test_t0 = 0, s_test_last = 0;
static float s_test_v0 = 0;
static int s_test_n = 0;
static char s_test_file[48];

void batt_test_start() {
  s_test = true;
  s_test_t0 = hal_millis();
  s_test_last = 0;
  s_test_v0 = hal_battery_volts();
  s_test_n = 0;
  char day[11];
  log_today(day);
  int h, mi;
  hal_time(&h, &mi);
  snprintf(s_test_file, sizeof(s_test_file), "/Waage/Akku/test_%s_%02d%02d.csv", day[0] ? day : "x", h, mi);
  storage_write(s_test_file, "Minute;Spannung;Prozent\n");
}

void batt_test_stop() {
  s_test = false;
}

bool batt_test_running() { return s_test; }
float batt_test_hours() { return s_test ? (hal_millis() - s_test_t0) / 3600000.0f : 0.0f; }
float batt_test_v_start() { return s_test_v0; }
int batt_test_samples() { return s_test_n; }

static void test_loop() {
  if (!s_test) return;
  uint32_t now = hal_millis();
  if (s_test_last && now - s_test_last < 60000) return;  // jede Minute ein Wert
  s_test_last = now;
  char line[48], v[12];
  fmt_num(v, sizeof(v), hal_battery_volts(), 3);
  snprintf(line, sizeof(line), "%lu;%s;%d", (unsigned long)((now - s_test_t0) / 60000), v,
           hal_battery_percent());
  storage_append(s_test_file, line);
  s_test_n++;
}

void batt_loop() {
  test_loop();
  uint32_t now = hal_millis();
  int pct = hal_battery_percent();
  if (pct < 0) return;
  if (s_count == 0) {  // erster Punkt sofort
    push(pct);
    s_last = now;
    return;
  }
  if (now - s_last < BATT_INTERVAL_MS) return;
  s_last = now;
  push(pct);

  // Tagesdatei auf der SD mitschreiben
  char day[11], line[48];
  log_today(day);
  int h, mi;
  hal_time(&h, &mi);
  bat_state_t st = hal_battery_state();
  const char *sn = st == BAT_CHARGING ? "laedt" : (st == BAT_FULL ? "voll" : "akku");
  snprintf(line, sizeof(line), "%02d:%02d;%d;%.2f;%s", h, mi, pct, hal_battery_volts(), sn);
  char path[48];
  snprintf(path, sizeof(path), "/Waage/Akku/%s.txt", day[0] ? day : "unbekannt");
  storage_append(path, line);
}

int batt_count() {
  return s_count;
}

int batt_percent_at(int i) {
  if (i < 0 || i >= s_count) return 0;
  return s_pct[i];
}

// Verbrauch aus den letzten zwei Stunden (24 Punkte) schätzen
float batt_per_hour() {
  int n = s_count < 24 ? s_count : 24;
  if (n < 4) return 0;
  int first = s_pct[s_count - n];
  int last = s_pct[s_count - 1];
  float hours = (n - 1) * (BATT_INTERVAL_MS / 1000.0f) / 3600.0f;
  if (hours <= 0) return 0;
  float per = (first - last) / hours;
  return per > 0 ? per : 0;  // beim Laden kein Verbrauch
}

float batt_hours_left() {
  int pct = hal_battery_percent();
  if (pct <= 0) return 0;
  float per = batt_per_hour();
  // Solange der eigene Verbrauch noch nicht gemessen ist, mit der Laufzeit
  // aus dem Entladetest rechnen: die Prozentzahl folgt ja der Restlaufzeit.
  if (per <= 0.2f) return pct / 100.0f * BAT_RUNTIME_H;
  return pct / per;
}
