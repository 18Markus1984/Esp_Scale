#pragma once
// ============================================================
//  Eigene Stimme: jede Ansage selbst aufnehmen
//
//  Ordner /Waage/Stimme/Eigene Stimme (englisch: Eigene Stimme EN).
//  Beim Anlegen werden alle Dateien einer Vorlage (gewählte Stimme,
//  Sätze der Sprachsteuerung aus _Zusatz) hineinkopiert, so ist das Paket
//  sofort vollständig. Danach ersetzt man einzelne Ansagen durch eigene
//  Aufnahmen – an der Waage über das Mikrofon oder im Browser über das
//  Handy. Was man sagt, ist frei ("Hallo Schatz, jetzt kannst du tara").
//  Welche Dateien schon eigene sind, steht in _eigen.txt im Ordner.
// ============================================================

#define OWN_GROUPS 4

typedef struct {
  const char *file;   // Dateiname ohne .wav
  const char *text;   // Vorschlag, was man sagen kann
  int group;          // 0 Ansagen, 1 Sprachsteuerung, 2 Bausteine und Einheiten, 3 Zahlen
  int max_s;          // längste Aufnahme in s
} own_slot_t;

const char *own_pack();                 // Ordnername für die eingestellte Sprache
bool own_exists();                      // Paket schon angelegt?
bool own_create(const char *from);      // anlegen, Vorlage kopieren (from = Paket, "" = gewählte Stimme)
int own_slots(const own_slot_t **out);  // Liste der Ansagen der eingestellten Sprache
bool own_is_mine(const char *file);     // schon selbst aufgenommen?
void own_mark(const char *file);        // als eigene Aufnahme merken
void own_path(char *buf, int len, const char *file);  // voller Pfad der WAV-Datei
int own_count_mine();                   // so viele sind schon eigene
const char *own_group_name(int g);
