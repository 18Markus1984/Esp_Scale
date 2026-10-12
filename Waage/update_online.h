#pragma once
// ============================================================
//  Online-Update von GitHub
//  Die Waage fragt die neueste Version aus den GitHub-Releases ab
//  (https://github.com/<OTA_REPO>/releases/latest), vergleicht sie
//  mit der eigenen Version (FW_VERSION) und lädt auf Wunsch die
//  Datei Waage.ino.bin aus dem Release und flasht sie (OTA).
//  Die Arbeit läuft in einer eigenen Task, die Anzeige bleibt bedienbar.
//  Gebaut und veröffentlicht werden die Releases von der GitHub Action
//  (.github/workflows/firmware.yml), siehe GITHUB.md.
// ============================================================
#include <stdint.h>

typedef enum {
  UPD_IDLE,       // noch nicht gesucht
  UPD_CONNECT,    // verbindet mit dem WLAN
  UPD_CHECK,      // fragt GitHub
  UPD_LATEST,     // installierte Version ist aktuell
  UPD_AVAILABLE,  // neuere Version gefunden
  UPD_DOWNLOAD,   // lädt und schreibt
  UPD_DONE,       // fertig, Neustart folgt
  UPD_ERROR,
} upd_state_t;

const char *fw_version();      // "1.4.2", lokal gebaut "0.0.0"
bool        fw_is_local();     // nicht von der GitHub Action gebaut
const char *upd_repo();        // "name/Esp_Scale"

void        upd_check();       // neueste Version suchen
void        upd_install();     // gefundene Version installieren (nur bei UPD_AVAILABLE)
upd_state_t upd_state();
bool        upd_busy();        // verbindet, sucht oder lädt gerade
const char *upd_latest();      // gefundene Version, z. B. "1.5.0"
int         upd_progress();    // 0 … 100 beim Laden
const char *upd_error();       // deutscher Text (für T())
void        upd_loop();        // aus dem UI-Takt: Ton, WLAN freigeben, Neustart
bool        upd_wants_wifi();  // WLAN nicht abschalten (Update läuft oder steht gleich an)
void        upd_restart();     // sauber neu starten (WLAN abmelden, SD aushängen, Display-Reset)

// Nach dem Einschalten (ui.cpp): still suchen, ohne Töne und Fehlermeldung
void        upd_check_quiet();
// Neue Firmware erst nach einem erfolgreichen Start als gültig markieren
// (OTA-Rollback: startet sie nicht sauber, kommt beim nächsten Einschalten die alte zurück)
void        upd_mark_valid();
bool        upd_pending_verify();  // laufende Firmware ist noch in der Probezeit
// Einmal nach dem Start: Text zum letzten Online-Update ("Aktualisiert auf Version %s"
// bzw. "Update fehlgeschlagen …") oder NULL. Text ist schon übersetzt.
const char *upd_boot_notice(bool *ok);
