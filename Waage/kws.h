#pragma once
// ============================================================
//  Schlüsselwort-Erkennung (KWS) ohne fremde Bibliothek
//
//  Merkmale: 16 kHz, Fenster 30 ms, Schritt 20 ms, 40 Mel-Bänder,
//  Logarithmus, gleitender Mittelwert je Band wird abgezogen
//  (macht die Erkennung unabhängig von der Lautstärke), dann die
//  Normierung aus dem Modell. Das Netz (kleines DS-CNN) sieht immer
//  die letzten 2 s (Länge steht im Modell) und läuft alle paar Schritte.
//
//  Das Modell liegt als Datei auf der SD-Karte
//  (/Waage/Sprache/kws_de.bin bzw. kws_en.bin) und wird mit dem
//  Werkzeug im Ordner Sprachmodell trainiert. Neues Modell = neue
//  Datei, kein Firmware-Update nötig. Dateiformat: siehe kws.cpp.
//
//  Klassen heißen nach ihrer Aktion: "hallo_waage", "tara",
//  "ziel_250" … Klassen mit "_" am Anfang (z. B. "_stille",
//  "_unbekannt") sind Hintergrund und lösen nie etwas aus.
// ============================================================
#include <stdint.h>
#include <stdbool.h>

#define KWS_RATE 16000
#define KWS_MAX_CLASSES 32

bool kws_load_file(const char *path);         // false: kws_error() sagt warum
bool kws_load_mem(const uint8_t *buf, int len);
void kws_unload();
bool kws_ready();
const char *kws_error();
const char *kws_info();                        // Text aus dem Modell (Datum, Beispiele)

int kws_num_classes();
const char *kws_class(int i);
int kws_class_index(const char *name);         // -1 = nicht im Modell
float kws_threshold();                         // Schwelle für Befehle
float kws_wake_threshold();                    // Schwelle für das Weckwort

void kws_reset();                              // Audio- und Merkmalsverlauf leeren
// Mono-PCM (16 kHz) hineinschieben. true = eine neue Auswertung liegt vor
bool kws_feed(const int16_t *pcm, int n);
const float *kws_probs();                      // geglättete Wahrscheinlichkeiten je Klasse
int kws_best(float *p);                        // beste Nicht-Hintergrund-Klasse (-1 = keine)
uint32_t kws_infer_us();                       // Dauer der letzten Auswertung

// Für Tests: Merkmale bzw. Netz direkt (Rückgabe: Anzahl Rahmen)
int kws_frames();
int kws_mels();
int kws_features(const int16_t *pcm, int n, float *out, int max_frames);
bool kws_run(const float *feat, float *probs);  // feat: kws_frames() x kws_mels(), normiert
