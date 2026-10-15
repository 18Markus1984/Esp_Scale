#pragma once
// ============================================================
//  Sprachbefehle
//
//  Standard (USE_VOICE 0): eigene Schlüsselwort-Erkennung (kws.*),
//  Modell von der SD-Karte (/Waage/Sprache/kws_de.bin), Weckwort
//  "Hallo Waage", danach einige Sekunden lang Befehle wie "Tara",
//  "Wie viel?", "Zweihundertfünfzig Gramm". Braucht keine eigene
//  Partition und kein ESP-SR.
//  Alt (USE_VOICE 1): ESP-SR mit englischen Befehlen, siehe voice.cpp.
// ============================================================
#include <stdint.h>

bool voice_available();  // Spracherkennung einkompiliert?
bool voice_begin();      // startet das Zuhören (Modell laden, Mikrofon)
void voice_stop();       // beendet es wieder
void voice_hold(bool on); // Online-Update: Mikrofon und Erkennung kurz anhalten (Rechenzeit, Speicher)
bool voice_starting();   // Start läuft gerade (Modell wird geladen)
bool voice_start_stuck(); // Start dauert ungewöhnlich lange (> 12 s)
bool voice_lack_memory(); // Start abgelehnt: zu wenig Speicher
bool voice_running();
bool voice_awake();    // Weckwort erkannt, wartet auf Befehl
bool voice_wake();     // wie "Hallo Waage" (Doppeltipp); false = Erkennung läuft nicht
void voice_loop();     // im UI-Takt aufrufen: führt erkannte Befehle aus
const char *voice_last();      // zuletzt verstandener Befehl ("" = noch keiner)
unsigned long voice_last_ms(); // wann (hal_millis)

// ---- eigenes Modell ----
bool voice_model_ok();             // Modell geladen (oder ladbar)
const char *voice_model_error();   // z. B. "kein Modell auf der SD-Karte"
const char *voice_model_info();    // Text aus dem Modell (Datum, Beispiele)
const char *voice_model_path();    // /Waage/Sprache/kws_de.bin bzw. kws_en.bin
void voice_model_changed();        // neue Modelldatei hochgeladen: beim nächsten Zuhören neu laden
// Mikrofonseite offen: Weckwort-Versuche (ab 25 %) als WAV nach /Waage/Sprache/Mitschnitt
// (w…wav = geweckt, k…wav = knapp daneben, b…_<befehl>_PP.wav = Befehl, n…wav = nicht verstanden;
//  Zahl im Namen = höchste Wahrscheinlichkeit)
void voice_capture(bool on);
int voice_capture_count();          // seit dem Einschalten gespeichert
const char *voice_capture_last();   // letzte Datei, z. B. "k003_48.wav"
int voice_live(float *p);          // Mikrofonseite: beste Klasse gerade (Index in voice_word…)
float voice_level();               // Pegel 0 … 1 (für Anzeigen)

// ---- Wörter aufnehmen (Trainingsdaten für das eigene Modell) ----
// Liste: feste Befehle + weitere Klassen des geladenen Modells
int voice_words();
const char *voice_word_id(int i);    // Ordnername, z. B. "hallo_waage"
const char *voice_word_text(int i);  // was man sagen soll, z. B. "Hallo Waage"
int voice_word_count(int i);         // vorhandene Aufnahmen (zählt auf der SD-Karte)
// Aufnahme starten (nach dem Piepton sprechen). false = Mikrofon/SD fehlt oder beschäftigt
// series: 30 s am Stück, das Wort mehrmals mit Pausen sagen (Datei s001.wav …)
bool voice_rec_start(int word, bool series = false);
void voice_rec_stop();               // Serie vorzeitig beenden und speichern
bool voice_rec_series();             // läuft/lief gerade eine Serie?
void voice_rec_session(bool on);     // Aufnahmeseite offen: Mikrofon bereithalten
// 0 bereit, 1 gleich geht's los, 2 nimmt auf, 3 speichert, 4 gespeichert, -1 Fehler
int voice_rec_state();
float voice_rec_progress();          // 0 … 1 während der Aufnahme
int voice_rec_ms(int word);          // Länge der Aufnahme

// Simulator: Audio direkt hineingeben (statt Mikrofon)
void voice_sim_feed(const int16_t *pcm, int n);
void voice_sim_cmd(const char *cls);  // Testumgebung: Befehl direkt ausführen
