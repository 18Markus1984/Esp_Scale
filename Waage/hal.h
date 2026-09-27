#pragma once
// ============================================================
//  Hardware-Schicht: Die UI fragt Sensoren nur über diese
//  Funktionen ab. So bleibt die UI unabhängig von den Treibern.
// ============================================================
#include <stdint.h>

void     hal_init();
uint32_t hal_millis();

// Beschleunigung der IMU (beliebige Einheit, nur Richtung zählt).
// false = keine gültigen Daten
bool hal_accel(float *x, float *y, float *z);

// Uhrzeit aus dem RTC
bool hal_time(int *hour, int *minute);

// Datum und Uhrzeit komplett; false = Uhr noch nicht gestellt (Jahr < 2024)
bool hal_now(int *year, int *month, int *day, int *hour, int *minute, int *second);

// Uhr stellen (schreibt in den RTC)
void hal_set_datetime(int year, int month, int day, int hour, int minute, int second);

// Akkustand in Prozent, -1 = unbekannt
int hal_battery_percent();
float hal_battery_volts();
#define BAT_RUNTIME_H 10.1f  // gemessene Laufzeit einer vollen Ladung in Stunden

// Ladezustand. Am USB hängt die Spannung am Ladegerät, dann ist der
// Prozentwert nicht der echte Ladestand -> Zustand mit anzeigen.
typedef enum { BAT_NONE, BAT_DISCHARGE, BAT_CHARGING, BAT_FULL } bat_state_t;
bat_state_t hal_battery_state();

// BOOT-Taste an der Seite (true = gedrückt)
bool hal_boot_pressed();

// ---- Stromversorgung (PWR-Taste + Selbsthaltung des Akkus) ----
// Die PWR-Taste schaltet den Akku in Hardware ein. Die Software hält die
// Versorgung über GPIO7 und kann sie zum Ausschalten wieder freigeben.
void hal_power_hold(bool on);   // true = Akku eingeschaltet halten
bool hal_pwr_pressed();         // PWR-Taste gedrückt?
void hal_backlight(int percent);
void hal_restart();
