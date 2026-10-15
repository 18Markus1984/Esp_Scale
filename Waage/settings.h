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
  bool speak;         // Gewicht/Stückzahl ansagen (Sprachdateien auf der SD)
  bool speak_ev;      // Sprüche des Stimmpakets (tara, Überlast, gespeichert …)
  bool voice_on;      // Spracherkennung (nur wirksam, wenn USE_VOICE 1)
  char voice[24];     // Stimmpaket: Unterordner in /Waage/Stimme
  int  unit;          // Einheit Wiegeseite/Protokoll: 0 g, 1 kg, 2 oz, 3 lb
  bool autosave;      // automatisch speichern, wenn das Gewicht ruhig liegt
  bool auto_next;     // in Rezept/Cocktail automatisch zum nächsten Schritt
  int  idle_min;      // nach so vielen Minuten ohne Bedienung zur Wiegeseite (0 = aus)
  int  auto_off_min;  // nach so vielen Minuten ohne Bedienung ausschalten (0 = nie)
  float lvl_off_x;    // Nullpunkt der Libelle in Grad
  float lvl_off_y;
  bool precise;       // Präzisionsmodus: ruhige Werte mitteln, zwei Nachkommastellen
  bool whole;         // ganze Gramm ohne Komma (wie im Profil Einfach); nie zusammen mit precise
  int theme;         // Farbschema: 0 Grün, 1 Weiß, 2 Blau, 3 Violett, 4 Pink, 5 Orange, 6 Gelb, 7 Rot
  int disp_rot;      // Display drehen, Zehntelgrad (Feinausrichtung im Gehäuse)
  bool azt;           // Nullpunkt-Nachführung (Auto-Null)
  int  liquid;        // Flüssigkeit für die Einheit ml (Index in data.cpp)
  int  lang;          // Sprache von Anzeige, Weboberfläche und Ansage: 0 Deutsch, 1 English
  // Prüfgewicht: regelmäßige Kontrolle mit einem bekannten Gewicht (refcheck.*)
  float ref_g;        // Sollgewicht, 0 = aus
  int   ref_days;     // Intervall in Tagen, 0 = keine Erinnerung
  float ref_tol;      // erlaubte Abweichung in g
  int   ref_last;     // Tag der letzten Prüfung (Tage seit 1.1.2000), 0 = noch nie
  float ref_meas;     // zuletzt gemessen
  bool  ref_ok;       // letzte Prüfung in Ordnung?
  // Profile und Kategorien (welche Modi auf dem Startbildschirm erscheinen)
  int      profile;   // PROFILE_STANDARD … PROFILE_EINFACH
  uint8_t  cats;      // sichtbare Gruppen: Bit 0 Küche, Bit 1 Werkstatt, Bit 2 Spiele
  uint16_t modes_off; // ausgeblendete Modi, Bit = Index über alle Gruppen
                      // (Küche 0–5, Werkstatt 6–11, Spiele 12–15)
  bool     setup_done; // Einrichtungs-Assistent erledigt (sonst startet er nach dem Startbild)
  bool     upd_auto;   // neue Firmware nach dem Einschalten automatisch installieren (sonst nachfragen)
} settings_t;

#define PROFILE_STANDARD  0
#define PROFILE_KUECHE    1
#define PROFILE_WERKSTATT 2
#define PROFILE_EINFACH   3
#define PROFILE_COUNT     4

#define CAT_KUECHE    1
#define CAT_WERKSTATT 2
#define CAT_SPIELE    4
#define CAT_ALL       7

#define LANG_DE 0
#define LANG_EN 1

extern settings_t g_set;

void settings_load();
void settings_save();

// Profil wählen und seine Voreinstellungen übernehmen (ohne zu speichern).
// Standard: alle Gruppen · Küche: nur Küche, Auto-Tara an · Werkstatt: nur
// Werkstatt · Einfach: nur Wiegen, Ansage an (Farbe bleibt)
void settings_profile_apply(int profile);

// Uhrzeit sichern und zurückholen (falls die RTC beim Ausschalten stromlos ist)
void settings_store_time(int y, int mo, int d, int h, int mi);
bool settings_load_time(int *y, int *mo, int *d, int *h, int *mi);
