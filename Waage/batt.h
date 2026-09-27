#pragma once
#include <stdint.h>
// ============================================================
//  Akkuverlauf: alle 5 Minuten ein Messpunkt, 12 Stunden im Speicher,
//  zusätzlich tageweise auf der SD-Karte (/Waage/Akku/<Datum>.txt).
//  Daraus werden Verbrauch pro Stunde und Restlaufzeit geschätzt.
// ============================================================

#define BATT_POINTS 144      // 144 * 5 min = 12 h
#define BATT_INTERVAL_MS 300000UL

void batt_loop();            // regelmäßig aufrufen
int  batt_count();           // vorhandene Messpunkte
int  batt_percent_at(int i); // 0 = ältester Punkt
float batt_per_hour();       // Verbrauch in Prozentpunkten pro Stunde (0 = unbekannt)
float batt_hours_left();     // geschätzte Restlaufzeit (0 = unbekannt)

// ---------------- Entladetest ----------------
// Misst einmal komplett durch: von voll bis zur Abschaltspannung. Dabei wird
// jede Minute ein Wert nach /Waage/Akku/test_<Datum>.csv geschrieben, damit
// sich die echte Laufzeit und der Kurvenverlauf auswerten lassen.
void  batt_test_start();
void  batt_test_stop();
bool  batt_test_running();
float batt_test_hours();     // Laufzeit seit Start
float batt_test_v_start();   // Spannung beim Start
int   batt_test_samples();
