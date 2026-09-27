#pragma once
// ============================================================
//  Weboberfläche (siehe Design-Sheet)
//   Rezepte, Töpfe/Spulen, Protokoll und Einstellungen am Handy
//   oder PC bearbeiten. Läuft nur auf Abruf: Nach 10 Minuten ohne
//   Zugriff schaltet sich der Server samt WLAN wieder ab.
//   Ohne bekanntes Heimnetz macht die Waage ein eigenes WLAN auf.
// ============================================================
#include <stdint.h>

void web_start();
void web_stop();
void web_loop();            // regelmäßig aufrufen (UI-Timer)
bool web_running();
bool web_ap_mode();         // eigenes WLAN statt Heimnetz?
const char *web_url();      // "http://waage.local" bzw. IP
const char *web_ssid();     // im AP-Modus der Netzname
const char *web_ip();       // IP-Adresse (falls waage.local nicht klappt)
uint32_t web_seconds_left();  // bis zur Abschaltung

// Update-Modus: hält WLAN und OTA für 15 Minuten wach, damit der Upload
// über die Arduino-IDE nicht am Abschalttimer scheitert
void web_update_mode(uint32_t minutes);
bool web_update_mode_active();
uint32_t web_update_seconds_left();
bool web_hide_weight();       // Schätzspiel läuft im Browser -> Gewicht am Display verstecken
