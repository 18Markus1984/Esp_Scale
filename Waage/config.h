#pragma once
// ============================================================
//  Waage – zentrale Einstellungen
// ============================================================

// 1 = simulierte Waage (ohne HX711, Werte laufen automatisch durch)
// 0 = echter HX711 an GPIO44 (DOUT) / GPIO43 (SCK)
#ifndef SIM_WAAGE
#define SIM_WAAGE 0
#endif

// Messbereich der Wägezelle in Gramm
#define WAAGE_MAX_G 3000.0f

// Bis zu diesem Winkel gilt die Waage beim Start als gerade
#define LEVEL_OK_DEG 2.0f

// So lange muss die Waage gerade stehen, bevor es automatisch weitergeht
#define LEVEL_OK_HOLD_MS 1000

// Nach Anzeige "gerade" noch so lange warten, dann zur Wiegeseite
#define LEVEL_CONTINUE_MS 1000

// Einbaulage des Displays im Gehäuse: 0 = 0°, 1 = 90°, 2 = 180°, 3 = 270°
// Nur ändern, wenn die Blase beim Anheben der Vorderkante in die falsche
// Richtung läuft. Die kleine Schräge gleicht danach "Libelle kalibrieren" aus.
#define DISPLAY_MOUNT 0

// Achsen des Lagesensors (QMI8658) für die Libelle: Auf der Platine liegen
// X und Y des Sensors quer zu X und Y des Displays, deshalb getauscht.
// Läuft die Blase danach auf einer Achse noch falsch herum, die passende
// Achse mit LEVEL_FLIP_X (links/rechts) bzw. LEVEL_FLIP_Y (vorne/hinten) umdrehen.
// Danach einmal Setup -> Waage -> Libelle kalibrieren.
#define LEVEL_SWAP_XY 1
#define LEVEL_FLIP_X  0
#define LEVEL_FLIP_Y  0

// Rezepte mit Eiern: Gramm je Ei ohne Schale (Größe M). Daraus ergibt sich die
// Anzahl Eier im Grundrezept, z. B. „Eier;110“ = 2 Eier. Gewogen wird danach echt.
#define EGG_G 55.0f

// Tiefentladeschutz: Ab dieser Spannung warnt die Waage, darunter schaltet
// sie ab. LiPo-Zellen nehmen unter etwa 3,2 V Schaden.
#define BAT_WARN_V 3.45f
#define BAT_CUTOFF_V 3.30f

// Takt der SD-Karte in kHz. Standard des Treibers wären 40000 (40 MHz);
// das ist im 1-Bit-Modus mit Lautsprecher und WLAN zu störanfällig.
// 10000 reicht für Protokoll und Sprachdateien locker.
#define SD_FREQ_KHZ 10000

// Auto-Speichern: so lange muss das Gewicht ruhig liegen, und so viel muss es mindestens sein
#define AUTOSAVE_STABLE_MS 2000
#define AUTOSAVE_MIN_G 5.0f

// Bluetooth-Tastatur für den Zählmodus (sendet Zahlen + Enter an den PC)
// 1 = an, 0 = aus (falls es beim Kompilieren Probleme mit Bluetooth gibt)
#ifndef USE_BLE_KBD
#define USE_BLE_KBD 1
#endif

// 1 = Mikrofon-Rohwerte im seriellen Monitor ausgeben (Fehlersuche)
#ifndef MIC_DEBUG
#define MIC_DEBUG 0
#endif

// Spracherkennung (ESP-SR): 1 = an. Dann in der Arduino-IDE das
// Partitionsschema "ESP SR 16M" wählen. Vorher System -> Mikrofon testen.
#ifndef USE_VOICE
#define USE_VOICE 0
#endif

// 1 = jedes Aufsetzen/Loslassen des Fingers im seriellen Monitor ausgeben
// (zur Fehlersuche bei Touch-Problemen)
#ifndef TOUCH_DEBUG
#define TOUCH_DEBUG 0
#endif

// ------------------------------------------------------------
//  Online-Update von GitHub (Setup -> Waage -> Firmware)
//  Hier deinen GitHub-Namen eintragen: "GitHub-Name/Esp_Scale".
//  Das Repository muss öffentlich sein, siehe GITHUB.md.
#ifndef OTA_REPO
#define OTA_REPO "18Markus1984/Esp_Scale"
#endif
// Name der Firmware-Datei im Release (so heißt sie nach dem Kompilieren)
#define OTA_ASSET "Waage.ino.bin"

// Version: die GitHub Action schreibt beim Bauen version.h (aus dem Tag,
// z. B. v1.4.2 -> "1.4.2"). Lokal in der Arduino-IDE gebaut fehlt die
// Datei, dann gilt "0.0.0" und jede Version auf GitHub ist neuer.
#if __has_include("version.h")
#include "version.h"
#endif
#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#define FW_LOCAL 1
#endif
