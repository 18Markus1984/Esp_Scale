#pragma once
// ============================================================
//  Eigene Stimme: jede Ansage selbst aufnehmen
//
//  Ordner /Waage/Stimme/Eigene Stimme (englisch: Eigene Stimme EN).
//  Beim Anlegen wird nur die Vorlage gemerkt (_basis.txt, z. B. die bisher
//  gewählte Stimme): was noch nicht aufgenommen ist, kommt beim Abspielen
//  aus der Vorlage. Danach ersetzt man einzelne Ansagen durch eigene
//  Aufnahmen – an der Waage über das Mikrofon oder im Browser über das
//  Handy. Was man sagt, ist frei ("Hallo Schatz, jetzt kannst du tara").
//  Welche Dateien schon eigene sind, steht in _eigen.txt im Ordner.
// ============================================================

#define OWN_GROUPS 4

typedef struct {
  const char *file;   // Dateiname ohne .wav
  const char *text;   // Vorschlag, was man sagen kann
  int group;          // 0 Ansagen, 1 Sprachsteuerung, 2 Wörter und Einheiten, 3 Zahlen
  int max_s;          // längste Aufnahme in s
  const char *when;   // wann die Waage das sagt (Erklärung für den Nutzer)
} own_slot_t;

const char *own_pack();                 // Ordnername für die eingestellte Sprache
bool own_exists();                      // Paket schon angelegt?
bool own_create(const char *from);      // anlegen (from = Vorlage, "" = gewählte Stimme), kopiert nichts
const char *own_base();                 // Vorlage: fehlende Ansagen kommen von hier ("" = _Zusatz)
bool own_is_pack(const char *pack);     // ist das der Ordner der eigenen Stimme?
int own_slots(const own_slot_t **out);  // Liste der Ansagen der eingestellten Sprache
bool own_is_mine(const char *file);     // schon selbst aufgenommen?
void own_mark(const char *file);        // als eigene Aufnahme merken
void own_path(char *buf, int len, const char *file);  // voller Pfad der WAV-Datei
int own_count_mine();                   // so viele sind schon eigene
const char *own_group_name(int g);
