// Umsetzung der Hardware-Schicht mit den Waveshare-Treibern
#include <Arduino.h>
#include "hal.h"
#include "Gyro_QMI8658.h"
#include "RTC_PCF85063.h"
#include "BAT_Driver.h"
#include "Display_SPD2010.h"
#include "esp_lcd_panel_ops.h"

extern esp_lcd_panel_handle_t panel_handle;  // aus Display_SPD2010.cpp

#define BOOT_PIN 0
#define PIN_PWR_KEY 6    // PWR-Taste, LOW = gedrückt
#define PIN_PWR_HOLD 7   // Selbsthaltung: HIGH = Akku bleibt an

void hal_init() {
  pinMode(BOOT_PIN, INPUT_PULLUP);
  pinMode(PIN_PWR_KEY, INPUT);
}

void hal_power_hold(bool on) {
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, on ? HIGH : LOW);
}

bool hal_pwr_pressed() {
  return digitalRead(PIN_PWR_KEY) == LOW;
}

void hal_backlight(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  Set_Backlight((uint8_t)percent);
}

void hal_restart() {
  // Display vor dem Neustart sauber abschalten und im Reset festhalten.
  // Sonst übernimmt der Display-Controller Reste der alten Sitzung und das
  // Bild ist nach dem Neustart verzerrt.
  Set_Backlight(0);
  esp_lcd_panel_disp_on_off(panel_handle, false);
  Set_EXIO(EXIO_PIN2, Low);  // Display-Reset
  Set_EXIO(EXIO_PIN1, Low);  // Touch-Reset
  delay(100);
  esp_restart();
}

uint32_t hal_millis() {
  return millis();
}

bool hal_accel(float *x, float *y, float *z) {
  // Accel wird im Treiber-Task laufend aktualisiert
  *x = Accel.x;
  *y = Accel.y;
  *z = Accel.z;
  return (*x != 0.0f || *y != 0.0f || *z != 0.0f);
}

bool hal_time(int *hour, int *minute) {
  *hour = datetime.hour;
  *minute = datetime.minute;
  return datetime.hour < 24 && datetime.minute < 60;
}

bool hal_now(int *year, int *month, int *day, int *hour, int *minute, int *second) {
  *year = datetime.year;
  *month = datetime.month;
  *day = datetime.day;
  *hour = datetime.hour;
  *minute = datetime.minute;
  *second = datetime.second;
  return datetime.year >= 2024 && datetime.month >= 1 && datetime.month <= 12;
}

// Wochentag 0 = Sonntag (Sakamoto)
static int weekday(int y, int m, int d) {
  static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

void hal_set_datetime(int year, int month, int day, int hour, int minute, int second) {
  datetime_t dt;
  dt.year = year;
  dt.month = month;
  dt.day = day;
  dt.dotw = weekday(year, month, day);
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  PCF85063_Set_All(dt);
  datetime = dt;  // Anzeige sofort aktualisieren
}

float hal_battery_volts() {
  return BAT_analogVolts;
}

// Ladezustand aus der Spannung, abgeglichen mit Messungen am Gerät:
//   über 4,19 V  -> Ladegerät schiebt aktiv (grüne Lade-LED an)
//   4,14..4,19 V -> Kabel steckt, Ladung beendet ("voll")
//   direkt nach dem Abziehen fällt die Spannung auf etwa 4,13 V
//   darunter     -> Akkubetrieb
// Wichtig ist die Vorgeschichte: 4,15 V mit Kabel heißt voll, dieselbe
// Spannung nach dem Abziehen heißt schlicht "fast voller Akku".
bat_state_t hal_battery_state() {
  static float v_ref = 0;
  static uint32_t t_ref = 0;
  static bat_state_t state = BAT_DISCHARGE;
  static float trend = 0;
  float v = BAT_analogVolts;
  if (v < 3.0f) return BAT_NONE;

  uint32_t now = millis();
  if (t_ref == 0) {
    v_ref = v;
    t_ref = now;
  }
  if (now - t_ref > 30000) {  // Trend alle 30 s
    trend = v - v_ref;
    v_ref = v;
    t_ref = now;
  }

  if (v >= 4.19f) {
    state = BAT_CHARGING;  // so hoch schafft es nur das Ladegerät
  } else if (v >= 4.14f) {
    // Kabel steckt und die Ladung ist fertig – außer die Spannung fällt
    // gerade deutlich, dann wurde eben abgezogen
    if (state == BAT_CHARGING || state == BAT_FULL) state = (trend < -0.01f) ? BAT_DISCHARGE : BAT_FULL;
    else state = (trend > 0.008f) ? BAT_CHARGING : BAT_DISCHARGE;
  } else {
    state = BAT_DISCHARGE;
  }
  return state;
}

// Entladekurve, gemessen am Gerät (Entladetest vom 23./24.09.2026, 10,1 h
// Laufzeit von 4,11 V bis 3,02 V). Die Prozentzahl entspricht dem Anteil der
// verbleibenden Laufzeit, nicht der Spannung: bei 3,75 V ist noch gut die
// Hälfte der Zeit übrig, obwohl die Spannung schon tief aussieht.
int hal_battery_percent() {
  float v = BAT_analogVolts;
  if (v < 2.5f) return -1;  // kein Akku erkannt
  if (v > 4.15f) v = 4.15f;  // 4,15 V ist der volle Akku ohne Kabel
  static const float CURVE[][2] = { { 3.20f, 0 },  { 3.30f, 3 },  { 3.35f, 6 },  { 3.40f, 9 },
                                    { 3.45f, 13 }, { 3.50f, 19 }, { 3.55f, 26 }, { 3.60f, 34 },
                                    { 3.65f, 41 }, { 3.70f, 50 }, { 3.75f, 60 }, { 3.80f, 69 },
                                    { 3.85f, 76 }, { 3.90f, 84 }, { 3.95f, 89 }, { 4.00f, 94 },
                                    { 4.05f, 98 }, { 4.15f, 100 } };
  const int n = sizeof(CURVE) / sizeof(CURVE[0]);
  if (v <= CURVE[0][0]) return 0;
  for (int i = 1; i < n; i++) {
    if (v <= CURVE[i][0]) {
      float f = (v - CURVE[i - 1][0]) / (CURVE[i][0] - CURVE[i - 1][0]);
      return (int)(CURVE[i - 1][1] + f * (CURVE[i][1] - CURVE[i - 1][1]) + 0.5f);
    }
  }
  return 100;
}

bool hal_boot_pressed() {
  return digitalRead(BOOT_PIN) == LOW;
}
