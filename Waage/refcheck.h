#pragma once
// ============================================================
//  Prüfgewicht: die Waage erinnert in einem einstellbaren Abstand
//  daran, ein bekanntes Gewicht aufzulegen, und speichert das
//  Ergebnis (Einstellungen + /Waage/Pruefung/pruefgewicht.csv).
//  Einstellen: Setup -> Waage -> Prüfgewicht, oder im Browser.
// ============================================================
#include <stdint.h>

int  refchk_today();                 // Tage seit 1.1.2000, -1 = Uhr nicht gestellt
int  refchk_days_left();             // bis zur nächsten Prüfung (<0 = überfällig), 9999 = keine Erinnerung
bool refchk_due();                   // jetzt fällig? (auch nach einer Prüfung außer Toleranz)
void refchk_date(int day, char *b, int len);  // Tag -> "12.10.2026"
bool refchk_store(float measured);   // Ergebnis speichern, Rückgabe: in Toleranz?
