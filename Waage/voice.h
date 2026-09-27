#pragma once
// ============================================================
//  Spracherkennung (ESP-SR). Siehe Kopf von voice.cpp:
//  USE_VOICE in config.h und Partitionsschema "ESP SR 16M".
// ============================================================

bool voice_available();  // ist ESP-SR einkompiliert? (USE_VOICE in config.h)
bool voice_begin();      // startet Weckwort-Erkennung (false = nicht verfügbar)
void voice_stop();       // beendet sie wieder
bool voice_starting();   // Start läuft gerade (Modelle werden geladen)
bool voice_start_stuck(); // Start dauert ungewöhnlich lange (> 12 s)
bool voice_lack_memory(); // Start abgelehnt: zu wenig interner Speicher
bool voice_running();
bool voice_awake();    // Weckwort erkannt, wartet auf Befehl
void voice_loop();     // im UI-Takt aufrufen: führt erkannte Befehle aus
const char *voice_last();      // zuletzt verstandener Befehl ("" = noch keiner)
unsigned long voice_last_ms(); // wann (hal_millis)
