#pragma once
// ============================================================
//  Waagen-Logik: Tara, Stabilität, Überlast, Kalibrierung.
//  Intern wird mit den Rohwerten (Zählwerten) des HX711 gerechnet:
//     Gramm = (Rohwert - Nullpunkt) / Faktor
//  Liefert bei SIM_WAAGE = 1 simulierte Werte.
// ============================================================

void  scale_init();
void  scale_update();     // ca. alle 100 ms aufrufen
float scale_gross();      // Bruttogewicht in g (geglättet, wie angezeigt)
float scale_gross_raw();  // ungefiltert (für Kalibrierung und Diagnose)
float scale_net();        // Nettogewicht in g (mit Tara)
bool  scale_stable();     // Wert ruhig?
bool  scale_overload();   // über Messbereich?
void  scale_tare();
float scale_get_tare();          // aktuelle Tara in g
void  scale_set_tare(float g);   // Tara direkt setzen (z. B. Topf abziehen)

// Präzisionsmodus: Mittelwert, solange das Gewicht ruhig liegt (bis 5 s).
// false = noch nicht genug ruhige Werte. u = Unsicherheit des Mittelwerts (±g, ca. 95 %)
bool  scale_precise(float *net, float *u, int *n);

// Nullpunkt-Nachführung an/aus (langsame Drift der leeren Waage automatisch auf 0)
void  scale_set_autozero(bool on);

// ---- Kalibrierung ----
// Bis zu drei Referenzpunkte: dazwischen wird linear umgerechnet, außerhalb
// mit der Steigung des äußersten Abschnitts weitergerechnet. Ein Punkt
// entspricht der bisherigen Kalibrierung über einen Faktor.
#define CAL_MAX 3
long  scale_raw();        // Rohwert, gemittelt über ca. 1 s
float scale_factor();     // Zählwerte pro Gramm
// Nullpunkt und Faktor aus zwei Messungen berechnen und im Flash speichern.
// Rückgabe false, wenn der Unterschied zu klein ist (kein Gewicht erkannt).
bool  scale_calibrate(long zero_raw, long load_raw, float ref_g);
// Mehrpunkt-Kalibrierung: Nullpunkt plus n Referenzpunkte (roh + Sollgewicht)
bool  scale_calibrate_points(long zero_raw, const long *raws, const float *grams, int n);
int   scale_cal_count();                       // gespeicherte Referenzpunkte
float scale_cal_gram(int i);                   // Sollgewicht des Punktes i
