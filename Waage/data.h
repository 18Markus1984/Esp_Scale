#pragma once
// ============================================================
//  Daten auf der SD-Karte: Töpfe, Protokoll, Rezepte
//  Dezimalzahlen in den Dateien mit Komma, Trennzeichen ";"
//  -> lassen sich direkt mit Excel öffnen und von Hand bearbeiten
// ============================================================
#include <stdint.h>

// ---------------- Töpfe ----------------
#define POT_MAX 20
#define POT_COLORS 4

typedef struct {
  char name[32];
  float grams;
  uint8_t color;     // 0..3, Farben siehe pot_color()
  char created[11];  // "2026-09-12"
} pot_t;

extern pot_t g_pots[POT_MAX];
extern int g_pot_count;

void pots_load();
bool pots_save();
uint32_t pot_color_hex(int color);
// passender Topf zum Gewicht (Toleranz: max(tol_g, 1 %)), -1 = keiner
int pot_match(float grams, float tol_g, int exclude);

// ---------------- Protokoll ----------------
typedef struct {
  char time[9];    // "14:32:05"
  float value;     // in der gespeicherten Einheit
  char unit[4];    // "g", "kg", "oz", "lb"
  char note[32];   // Topfname oder Rezept
} log_entry_t;

// ---------------- Einheiten (Wiegeseite + Protokoll) ----------------
#define UNIT_COUNT 5
#define UNIT_ML 4                          // Milliliter über die Dichte der gewählten Flüssigkeit
const char *unit_name(int unit);          // "g", "kg", "oz", "lb", "ml"

// Flüssigkeiten für die Einheit ml (g_set.liquid)
int         liquid_count();
const char *liquid_name(int i);
float       liquid_density(int i);        // g/ml
int   unit_index(const char *name);       // umgekehrt, unbekannt = 0
float unit_from_g(float grams, int unit);
int   unit_decimals(int unit);            // g 1, kg 3, oz 2, lb 3, ml 1
// Wert (bereits in der Einheit) mit passenden Nachkommastellen und Komma
void  unit_fmt(char *buf, int len, float value, int unit);

// Schreibt "14:32:05;1234,5;g;Großer Topf" in die Datei des heutigen Tages
// (Gewicht in der eingestellten Einheit).
// Rückgabe: 1 = ok, 0 = keine SD-Karte, -1 = Uhr nicht gestellt (landet in unbekannt.txt)
int log_add(float grams, const char *note);

// Tage mit Protokoll, neueste zuerst: "2026-09-18"
int log_days(char days[][11], int max);
// Einträge eines Tages, neueste zuerst. *total = Anzahl Einträge in der Datei
int log_read(const char *day, log_entry_t *out, int max, int *total);
void log_today(char day[11]);  // "2026-09-18" oder "" wenn Uhr nicht gestellt

// ---------------- Rezepte ----------------
#define RECIPE_MAX_ING 20

typedef struct {
  char name[32];
  float grams;
} ingredient_t;

typedef struct {
  char file[48];
  char name[40];
  int portions;
  int count;
  ingredient_t ing[RECIPE_MAX_ING];
} recipe_t;

int  recipes_list(char files[][48], char names[][40], int counts[], int max);
bool recipe_load(const char *file, recipe_t *r);
// dieselben Funktionen für einen anderen Ordner (Cocktails)
int  recipes_list_dir(const char *dir, char files[][48], char names[][40], int counts[], int max);
bool recipe_load_dir(const char *dir, const char *file, recipe_t *r);
bool recipe_save_dir(const char *dir, const recipe_t *r, char *file_out, int len);
bool recipe_delete_dir(const char *dir, const char *file);

// ---------------- Cocktails ----------------
// Mengen stehen in ml. Die Waage rechnet über die Dichte der Zutat in Gramm um.
float ingredient_density(const char *name);   // g/ml, unbekannt = 1,0
void cocktails_create_examples();             // Beispiele, wenn der Ordner leer ist
void recipes_create_example();  // legt ein Beispielrezept an, wenn der Ordner leer ist

// ---------------- Porto ----------------
#define PORTO_MAX 10
#define FILE_PORTO "/Waage/Porto/porto.txt"

typedef struct {
  char name[24];
  int max_g;
  int price_ct;   // Preis in Cent
} porto_t;

extern porto_t g_porto[PORTO_MAX];
extern int g_porto_count;

void porto_load();   // aus der Datei, sonst Standardwerte (Stand 2026)
bool porto_save();   // sortiert nach Gewicht und speichert
void porto_defaults();

// ---------------- Leerspulen (Filament) ----------------
#define SPOOL_MAX 12
#define FILE_SPOOLS "/Waage/Spulen/spulen.txt"

typedef struct {
  char name[24];
  float grams;  // Gewicht der leeren Spule
} spool_t;

extern spool_t g_spools[SPOOL_MAX];
extern int g_spool_count;
void spools_load();
bool spools_save();

// ---------------- Spieler (Schätzspiel, Trinkspiel, Weboberfläche) ----------------
#define PLAYER_MAX 8
#define FILE_PLAYERS "/Waage/Spiel/spieler.txt"
extern char g_player_names[PLAYER_MAX][20];
extern int g_player_count;
void players_load();   // ohne Datei: "Spieler 1..3"
bool players_save();
const char *player_name(int i);  // leerer Name -> "Spieler i+1"
int  player_find(const char *name);            // -1 wenn unbekannt

// Wer spielt gerade mit? Die Namen selbst werden nur einmal gepflegt.
extern bool g_player_in[PLAYER_MAX];
int  players_pick_count();        // Anzahl gewählter Mitspieler
int  players_pick_index(int nth); // Listenindex des n-ten Mitspielers
void players_pick_default();      // beim ersten Mal die ersten drei wählen
bool player_add(const char *name);             // neuen Spieler anlegen und speichern

// ---------------- Bestenliste ----------------
// Eine Datei für alle Spiele: Datum;Spiel;Name;Wert;Einheit
// Wert ist die Abweichung bzw. Punktzahl; bei beiden Spielen gilt: kleiner ist besser.
#define FILE_SCORES "/Waage/Spiel/bestenliste.txt"
#define SCORE_MAX 200

typedef struct {
  char day[11];
  char game[12];
  char name[20];
  float value;
  char unit[4];
} score_t;

bool score_add(const char *game, const char *name, float value, const char *unit);
int  scores_load(score_t *out, int max);  // neueste zuerst

// ---------------- Ergebnisse der Modi mitschreiben ----------------
// In den Modi-Seiten regelmäßig aufrufen. Schreibt einmal pro Auflegen ins
// Protokoll, sobald das Gewicht 1,5 s ruhig liegt; neu erst, wenn die Waage
// wieder leer war.
void track_reset();
bool track_update(float net, bool stable, const char *note);  // true = gerade gespeichert

// Rezepte anlegen / löschen
bool recipe_save(const recipe_t *r, char *file_out, int len);  // neuer Dateiname aus dem Namen
bool recipe_delete(const char *file);

// Hilfen
float parse_num(const char *s);                 // "1234,5" oder "1234.5"
void  fmt_num(char *buf, int len, float v, int decimals);  // mit Komma
