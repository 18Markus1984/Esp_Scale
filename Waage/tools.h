#pragma once
// ============================================================
//  Dienste, die im Hintergrund weiterlaufen, egal welche Seite
//  offen ist: Küchentimer und Langzeitmessung.
//  tools_tick() läuft im UI-Takt (alle 100 ms, aus ui.cpp).
// ============================================================
#include <stdint.h>

void tools_tick();
bool tools_keep_awake();  // läuft etwas? Dann nicht automatisch ausschalten

// ---- Küchentimer (bis zu drei gleichzeitig) ----
#define TIMER_COUNT 3
void        timer_start(int i, uint32_t secs, const char *name);  // name darf NULL sein
void        timer_stop(int i);
void        timer_add(int i, int secs);    // laufenden Timer verlängern/verkürzen
bool        timer_running(int i);
bool        timer_ringing(int i);          // abgelaufen, klingelt noch
int         timer_left(int i);             // Restzeit in s, -1 = läuft nicht
uint32_t    timer_duration(int i);         // eingestellte Dauer in s
const char *timer_name(int i);             // "Timer 1" oder eigener Name (z. B. "Topfzeit")
int         timer_next();                  // laufender Timer mit der kürzesten Restzeit, -1 = keiner
int         timer_free();                  // erster freier Timer, -1 = alle belegt
void        timer_fmt(char *b, int len, int secs);  // 75 -> "1:15", 3725 -> "1:02:05"

// ---- Langzeitmessung ----
// Schreibt alle interval_s Sekunden das Nettogewicht nach
// /Waage/Messung/JJJJ-MM-TT_HHMM.csv (Uhrzeit;Minuten;Gewicht_g)
#define LT_HIST 120              // so viele Punkte merkt sich die Anzeige
bool        lt_start(int interval_s);   // false = keine SD-Karte
void        lt_stop();
bool        lt_running();
int         lt_interval();
int         lt_samples();
float       lt_hours();                 // Laufzeit seit Start
float       lt_first();                 // erster Messwert
float       lt_trend();                 // Steigung der letzten Punkte in g/h
const char *lt_file();                  // Dateiname ohne Ordner
int         lt_hist(float *out, int max);  // letzte Werte (alt -> neu), Rückgabe: Anzahl
