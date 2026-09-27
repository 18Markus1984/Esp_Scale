#pragma once
// ============================================================
//  Mikrofon (MSM261 am I2S-Port 1, Pins BCK 15 / WS 2 / DIN 39)
//  Wird für den Mikrofon-Test und für die Spracherkennung benutzt.
//  Der Lautsprecher hängt an I2S-Port 0 und stört nicht.
// ============================================================

bool mic_begin();        // I2S einrichten (einmalig)
// Die Spracherkennung übernimmt das Mikrofon exklusiv; dann darf niemand
// sonst davon lesen, sonst kommen sich zwei Leser in die Quere.
void mic_set_exclusive(bool on);
bool mic_exclusive();
bool mic_idle();         // liest der Pegel-Task gerade nicht?
void mic_listen(bool on);  // Pegelmessung an/aus (nur für den Test nötig)
float mic_level_db();    // aktueller Pegel in dBFS (-90 = still)
float mic_peak_db();     // Spitzenwert der letzten Sekunde
int  mic_claps();        // Zähler erkannter Klatscher
int  mic_channel();      // welcher Kanal Signal führt (0 = links, 1 = rechts)
int  mic_raw_peak();     // Spitze in Zählwerten (0 = Mikrofon liefert nichts)
bool mic_alive();        // kommen gerade Daten vom Mikrofon?
int  mic_restarts();     // wie oft der I2S-Empfang neu gestartet wurde
void mic_reset_claps();
