#pragma once
// ============================================================
//  PWR-Taste: Standby und Ausschalten
//   kurz drücken   -> Standby (gedimmte Uhr) bzw. wieder an
//   3 s halten     -> ausschalten (Akku wird freigegeben)
//  Aufwecken aus dem Standby: PWR-Taste, Display antippen oder
//  Gewicht auflegen.
// ============================================================

#define BRIGHT_ON 60   // Helligkeit im Betrieb (%)

void ui_power_init();   // nach dem Aufbau der Oberfläche
void ui_power_tick();   // alle 100 ms aus dem UI-Timer aufrufen
void ui_power_lang_changed();  // feste Texte nach einem Sprachwechsel neu setzen
bool ui_power_standby();
void ui_power_wake();   // aus dem Standby holen
