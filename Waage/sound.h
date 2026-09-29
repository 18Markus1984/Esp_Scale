#pragma once
// ============================================================
//  Töne und Sprachausgabe über den Lautsprecher (PCM5101, I2S)
//
//  Töne werden in einem eigenen Task erzeugt. Ein neuer Ton bricht
//  einen laufenden kurzen Ton ab, damit beim schnellen Tippen nichts
//  hinterherhängt. Lange Signale (Ziel erreicht, Trommelwirbel,
//  Ansagen) laufen geschützt zu Ende.
//
//  Sprachbausteine liegen als 16-kHz-Mono-WAV auf der SD-Karte:
//  /Waage/Stimme/<Paket>/*.wav  (null.wav, eins.wav, ... gramm.wav)
//  Englische Pakete haben englische Dateinamen (zero.wav ... grams.wav),
//  daran erkennt die Waage die Sprache eines Pakets (siehe stimme_erzeugen.py).
// ============================================================

typedef enum {
  SND_ROTARY,    // Rasten des Buchstabenrings
  SND_CLICK,     // Tastenklick
  SND_TARA,
  SND_SAVE,
  SND_WARN,
  SND_OVERLOAD,
  SND_TICK,
  SND_POT,
  SND_DONE,
  SND_PARK,
  SND_REACHED,
  SND_TEST,
  SND_DRUM
} sound_t;

// Tonschema (Einstellung "Ton")
#define SCHEME_COUNT 4
const char *scheme_name(int scheme);  // Klassisch, Sanft, Retro, Minimal

void sound_begin();
void sound_play(sound_t s);

// Parkpiepser: im Ziel-/Rezeptmodus alle 100 ms aufrufen
void sound_parking(float grams, float goal, float tol);
void sound_parking_reset();

// ---- Sprachausgabe ----
bool sound_voice_available();                      // Stimme auf der SD gefunden?
// Stimmpakete (Unterordner in /Waage/Stimme), Rückgabe: Anzahl
#define VOICE_PACKS_MAX 32                          // so viele Pakete zeigt die Auswahl
int  sound_voice_packs(char names[][24], int max);   // alphabetisch, nur passende Sprache
void sound_voice_next();                           // nächstes Paket wählen
void sound_voice_set(const char *pack);            // bestimmtes Paket wählen
void sound_lang_changed();                         // nach dem Sprachwechsel: passendes Paket wählen
void sound_speak_weight(float grams, int unit);    // "eintausend­zweihundert­vierunddreißig Gramm"
void sound_speak_count(int pieces);                // "siebenundvierzig Stück"
void sound_speak_word(const char *file);           // feste Ansage, z. B. "tara"
