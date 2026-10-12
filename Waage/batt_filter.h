#pragma once
#include <stdint.h>
// ============================================================
//  Akkuspannung glätten
//
//  Der ADC liefert alle 500 ms einen Wert. Der schwankt um einige
//  10 mV (Rauschen, WLAN, Lautsprecher, Hintergrundbeleuchtung); bei
//  3,7 V sind 10 mV schon 2 %. Deshalb:
//    1. Median aus den letzten 5 Werten (einzelne Ausreißer fallen weg)
//    2. gleitender Mittelwert: schnell (≈ 3 s) für den Ladezustand
//       (Kabel ein/aus), langsam (≈ 30 s) für die Prozentanzeige
//    3. Prozent mit Hysterese: beim Entladen geht die Anzeige nur nach
//       unten, beim Laden nur nach oben; Gegenrichtung erst ab 3 Punkten.
// ============================================================

void  bf_feed(float volts, uint32_t now_ms);  // neuer Messwert (ungefiltert)
bool  bf_ready();                             // mindestens ein Wert da
float bf_fast();                              // für Lade-/Entladeerkennung
float bf_slow();                              // für Prozent und Anzeige in Volt
// Rohprozent (aus bf_slow) -> angezeigte Prozent. charging: Kabel steckt
int   bf_percent(int raw_pct, bool charging);
void  bf_reset();                             // für Tests
