#pragma once
// ============================================================
//  Messmittelprüfung (Verfahren 1): Auswertung und Ablage
//
//  Jede Prüfung – an der Waage oder im Browser – landet als CSV in
//  /Waage/Pruefung/<Datum>_<Uhrzeit>.csv (Spalten mit ;, Komma als
//  Dezimalzeichen wie bei der Langzeitmessung). Oben Kennwerte und die
//  Kalibrierung, die zur Prüfung galt, darunter die Einzelwerte. Die
//  Weboberfläche liest die Dateien und zeichnet die Diagramme.
// ============================================================
#include <stdbool.h>

#define MSA_DIR "/Waage/Pruefung"

typedef struct {
  int n;
  double mean, bias, s, cg, cgk, tmin, lo, hi;
  bool ok;  // Cg und Cgk >= 1,33
} msa_stats_t;

// Kennwerte: Cg = 0,2·T / (6·s), Cgk = (0,1·T − |Bias|) / (3·s), T_min = 40·s + 10·|Bias|
void msa_calc(const float *v, int n, float nominal, float tol, msa_stats_t *r);

// Prüfung speichern. src = "Waage" oder "Browser". name bekommt den Dateinamen
// (ohne Ordner). false = keine SD-Karte oder Schreibfehler.
bool msa_save(float nominal, float tol, const float *v, int n, const char *src, char *name, int name_len);
