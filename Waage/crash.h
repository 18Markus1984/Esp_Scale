#pragma once
// ============================================================
//  Absturzbericht
//
//  Beim Start: Warum ist die Waage neu gestartet? Nach einem Absturz,
//  Watchdog oder Spannungseinbruch steht eine Zeile in
//  /Waage/Absturz.txt (SD-Karte) und im seriellen Monitor, mit Task,
//  Grund und Rücksprungadressen aus dem Core-Dump (lassen sich mit der
//  Waage.ino.elf aus dem GitHub-Release einer Code-Zeile zuordnen).
//  crash_phase() merkt sich, was gerade lief (z. B. "Beim Laden des Updates", 37 %);
//  das überlebt den Neustart nach einem Absturz.
// ============================================================

void crash_begin();               // einmal beim Start, nach storage_begin()
const char *crash_text();         // "" = normal eingeschaltet, sonst kurze Beschreibung
bool crash_last_abnormal();       // letzter Neustart: Absturz, Watchdog oder Spannungseinbruch
void crash_phase(const char *what, int pct);  // laufende Tätigkeit (NULL = nichts)
// Einzeln für die Weboberfläche (deutsche Texte aus fester Liste, dort übersetzt)
const char *crash_reason();       // "" oder z. B. "Absturz"
const char *crash_where();        // "" oder z. B. "Beim Laden des Updates"
int crash_pct();
