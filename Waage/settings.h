#pragma once
// ============================================================
//  Einstellungen (im internen Flash, bleiben auch ohne SD-Karte)
// ============================================================
#include <stdint.h>

typedef struct {
  bool autotara;      // Topferkennung mit Auto-Tara
  int  countdown_s;   // Countdown bis zur Auto-Tara
  int  tol_g;         // Toleranz der Topferkennung (mindestens 1 %)
  char ssid[33];      // WLAN
  char pass[65];
  int  volume;        // Lautstärke 0..100 (0 = aus)
  int  scheme;        // Tonschema 0..3 (Klassisch, Sanft, Retro, Minimal)
  bool speak;         // Gewicht ansagen (Sprachdateien auf der SD)
  bool voice_on;      // Spracherkennung (nur wirksam, wenn USE_VOICE 1)
  char voice[24];     // Stimmpaket: Unterordner in /Waage/Stimme
  int  unit;          // Einheit Wiegeseite/Protokoll: 0 g, 1 kg, 2 oz, 3 lb
  bool autosave;      // automatisch speichern, wenn das Gewicht ruhig liegt
  bool auto_next;     // in Rezept/Cocktail automatisch zum nächsten Schritt
  int  idle_min;      // nach so vielen Minuten ohne Bedienung zur Wiegeseite (0 = aus)
  int  auto_off_min;  // nach so vielen Minuten ohne Bedienung ausschalten (0 = nie)
  float lvl_off_x;    // Nullpunkt der Libelle in Grad
  float lvl_off_y;
  bool precise;
  bool azt;
  int  liquid;        // Flüssigkeit für die Einheit ml (Index in data.cpp)           // Nullpunkt-Nachführung (Auto-Null)      // Präzisionsmodus: ruhige Werte mitteln, zwei Nachkommastellen
  int  lang;          // Sprache von Anzeige, Weboberfläche und Ansage: 0 Deutsch, 1 English
} settings_t;

#define LANG_DE 0
#define LANG_EN 1

extern settings_t g_set;

void settings_load();
void settings_save();

// Uhrzeit sichern und zurückholen (falls die RTC beim Ausschalten stromlos ist)
void settings_store_time(int y, int mo, int d, int h, int mi);
bool settings_load_time(int *y, int *mo, int *d, int *h, int *mi);
