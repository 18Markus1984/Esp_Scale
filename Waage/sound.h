#pragma once
// ============================================================
//  Töne und Sprachausgabe über den Lautsprecher (PCM5101, I2S)
//
//  Töne werden in einem eigenen Task erzeugt. Ein neuer Ton bricht
//  einen laufenden kurzen Ton ab, damit beim schnellen Tippen nichts
//  hinterherhängt. Lange Signale (Ziel erreicht, Trommelwirbel)
//  laufen geschützt zu Ende. Ansagen stehen in einer eigenen
//  Warteschlange und werden nacheinander ganz gesprochen; nur
//  sound_speak_stop() (Lautsprecher-Knopf) bricht sie ab.
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
  SND_DRUM,
  SND_POWER_ON,  // Einschalten: Akkord blüht langsam auf (2,4 s), Taste darf jetzt los
  SND_POWER_OFF  // Ausschalten: derselbe Akkord verglüht (1,2 s)
} sound_t;

// Tonschema (Einstellung "Ton")
#define SCHEME_COUNT 4
const char *scheme_name(int scheme);  // Klassisch, Sanft, Retro, Minimal

void sound_begin();  // mehrfach aufrufbar: erster Aufruf startet I2S und Ton-Task, spätere prüfen nur die Stimme neu
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
// Auf Wunsch (Bildschirm angetippt): sofort ansagen, auch wenn die automatische
// Ansage aus ist, laufende Ansage wird abgebrochen
void sound_speak_weight_now(float grams, int unit);
// Zielwiegen: "noch 120 Gramm" / "20 Gramm zu viel" (now = sofort, auf Wunsch).
// false = Stimmpaket hat die Wörter noch nicht, dann lieber das Gewicht ansagen
bool sound_speak_rest(float rest_g, bool now);
void sound_speak_count(int pieces);                // "siebenundvierzig Stück"
void sound_speak_word(const char *file);           // feste Ansage, z. B. "tara"
void sound_speak_weight_word(float grams, int unit, const char *word);  // "… Gramm, gespeichert"
void sound_play_tone(sound_t s);                   // nur der Ton, ohne Sprachansage
bool sound_speaking();                             // Ansage läuft oder wartet
bool sound_busy();                                 // Ton oder Ansage läuft (Spracherkennung pausiert)
void sound_speak_stop();                           // laufende und wartende Ansagen abbrechen

// ---- Rückmeldungen der Sprachsteuerung ----
// Werden immer gesprochen (auch wenn die Sprüche aus sind) und unterbrechen
// eine laufende Ansage. false = keine Stimme oder ein Wort fehlt (dann lieber
// ein Ton). Fehlt ein Wort im gewählten Paket, nimmt die Waage es aus
// /Waage/Stimme/_Zusatz (englisch _Zusatz-EN).
bool sound_say(const char *words);                 // "ja", "nicht_verst", "hilfe", "ziel_beendet" (mit Leerzeichen mehrere)
bool sound_say_target(int grams);                  // "Ziel 250 Gramm. Jetzt einfüllen."
bool sound_say_battery(int pct, bool charging);    // "Akku 60 Prozent (wird geladen)"
bool sound_say_time(int h, int mi);                // "vierzehn Uhr fünf"
bool sound_repeat();                               // letzte Ansage (Gewicht, Restmenge …) nochmal

// Ansage-Modus: Zahlen = Gewicht/Stückzahl vorlesen (g_set.speak),
// Sprüche = Ereignisse des Stimmpakets wie "tara", "Überlast" (g_set.speak_ev)
#define SPEAK_ALL     0
#define SPEAK_NUMBERS 1
#define SPEAK_LINES   2
#define SPEAK_OFF     3
int  sound_speak_mode();
void sound_speak_mode_set(int mode);
const char *sound_speak_mode_name(int mode);       // deutscher Text, Anzeige über T()
// sound_play() sagt bei eingeschalteter Ansage zusätzlich das passende Wort:
// Tara -> "tara", Speichern -> "gespeichert", Topf -> "topf_erkannt",
// Ziel -> "ziel_erreicht", Fertig -> "fertig" (Überlast und "Waage leer" ruft ui.cpp auf)
