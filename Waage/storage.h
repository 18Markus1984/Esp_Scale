#pragma once
// ============================================================
//  SD-Karte: einfache Dateifunktionen
//
//  Ordner auf der Karte:
//   /Waage/Protokoll/2026-09-18.txt   eine Datei pro Tag
//   /Waage/Toepfe/toepfe.txt          alle Töpfe
//   /Waage/Rezepte/*.txt              ein Rezept pro Datei
//   /Waage/Porto/                     (später: Portotabelle)
//   /Waage/Sounds/                    (später: Soundpakete)
// ============================================================

#define DIR_ROOT      "/Waage"
#define DIR_LOG       "/Waage/Protokoll"
#define DIR_POTS      "/Waage/Toepfe"
#define DIR_RECIPES   "/Waage/Rezepte"
#define DIR_PORTO     "/Waage/Porto"
#define DIR_SOUNDS    "/Waage/Sounds"
#define DIR_SPOOLS    "/Waage/Spulen"
#define DIR_GAME      "/Waage/Spiel"
#define DIR_VOICE     "/Waage/Stimme"
#define DIR_COCKTAILS "/Waage/Cocktails"
#define DIR_BATT      "/Waage/Akku"
#define DIR_MSA       "/Waage/Pruefung"
#define FILE_POTS     "/Waage/Toepfe/toepfe.txt"

#define STORAGE_NAME_LEN 48

bool storage_begin();   // Karte einhängen, Ordner anlegen
bool storage_ok();      // Karte vorhanden?
void storage_loop();    // regelmäßig aufrufen: bemerkt Ausfälle und hängt die Karte neu ein
unsigned long storage_error_count();  // Zahl der bemerkten Kartenausfälle

// Für direkte Dateizugriffe außerhalb dieses Moduls (Ton-Task, Webserver):
// SD-Karte sperren, damit nie zwei Tasks gleichzeitig darauf zugreifen
void storage_lock();
void storage_unlock();

// Liest die Datei (bei großen Dateien nur das Ende) in buf, nullterminiert.
// Rückgabe: Anzahl Bytes, -1 bei Fehler
int  storage_read(const char *path, char *buf, int maxlen);
bool storage_write(const char *path, const char *data);    // überschreibt
bool storage_append(const char *path, const char *line);   // hängt Zeile + \n an
bool storage_remove(const char *path);
bool storage_exists(const char *path);
bool storage_mkdir(const char *path);

// Dateinamen (ohne Ordner) eines Verzeichnisses, unsortiert
int  storage_list(const char *dir, char names[][STORAGE_NAME_LEN], int max);
// Nur die Unterordner eines Verzeichnisses (z. B. Stimmpakete)
int  storage_list_dirs(const char *dir, char names[][STORAGE_NAME_LEN], int max);
