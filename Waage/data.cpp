#include "data.h"
#include "i18n.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include "storage.h"
#include "hal.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

pot_t g_pots[POT_MAX];
int g_pot_count = 0;

// Gemeinsamer Lesepuffer (nur aus dem UI-Task benutzen)
#define BUF_LEN 16384
static char *buf() {
  static char *b = NULL;
#ifdef ARDUINO
  if (!b) b = (char *)ps_malloc(BUF_LEN);  // externer PSRAM, internen RAM schonen
#endif
  if (!b) b = (char *)malloc(BUF_LEN);
  return b;
}

// ------------------------------------------------------------
float parse_num(const char *s) {
  char tmp[24];
  int i = 0;
  while (*s == ' ') s++;
  while (*s && i < 23) {
    tmp[i++] = (*s == ',') ? '.' : *s;
    s++;
  }
  tmp[i] = 0;
  return (float)atof(tmp);
}

void fmt_num(char *b, int len, float v, int decimals) {
  static const char *const F[] = { "%.0f", "%.1f", "%.2f", "%.3f" };
  snprintf(b, len, F[decimals < 0 ? 0 : (decimals > 3 ? 3 : decimals)], v);
  for (char *p = b; *p; p++)
    if (*p == '.') *p = ',';
}

// Zeile für Zeile durchgehen; line wird in-place zerlegt
typedef void (*line_cb)(char *line, void *ctx);
static void each_line(char *text, line_cb cb, void *ctx) {
  char *p = text;
  while (*p) {
    char *e = strchr(p, '\n');
    if (e) *e = 0;
    int l = strlen(p);
    if (l > 0 && p[l - 1] == '\r') p[l - 1] = 0;
    if (*p && *p != '#') cb(p, ctx);
    if (!e) break;
    p = e + 1;
  }
}

// Feld n (0-basiert) einer ";"-Zeile
static void field(const char *line, int n, char *out, int len) {
  const char *p = line;
  for (int i = 0; i < n && p; i++) {
    p = strchr(p, ';');
    if (p) p++;
  }
  out[0] = 0;
  if (!p) return;
  int i = 0;
  while (*p && *p != ';' && i < len - 1) out[i++] = *p++;
  out[i] = 0;
}

// ------------------------------------------------------------
//  Töpfe:  Name;Gewicht;Farbe;Datum
// ------------------------------------------------------------
uint32_t pot_color_hex(int c) {
  static const uint32_t COLORS[POT_COLORS] = { 0x3DDC97, 0xF5B83D, 0x7FB2F0, 0xE88AB4 };
  return COLORS[(c < 0 || c >= POT_COLORS) ? 0 : c];
}

static void pot_line(char *line, void *ctx) {
  if (g_pot_count >= POT_MAX) return;
  pot_t *p = &g_pots[g_pot_count];
  char f[24];
  field(line, 0, p->name, sizeof(p->name));
  field(line, 1, f, sizeof(f));
  p->grams = parse_num(f);
  field(line, 2, f, sizeof(f));
  p->color = (uint8_t)atoi(f) % POT_COLORS;
  field(line, 3, p->created, sizeof(p->created));
  if (p->name[0] && p->grams > 0) g_pot_count++;
}

void pots_load() {
  g_pot_count = 0;
  char *b = buf();
  if (!b || storage_read(FILE_POTS, b, BUF_LEN) < 0) return;
  each_line(b, pot_line, NULL);
}

bool pots_save() {
  char *b = buf();
  if (!b) return false;
  int n = snprintf(b, BUF_LEN, "# Name;Gewicht in g;Farbe 0-3;angelegt\n");
  for (int i = 0; i < g_pot_count && n < BUF_LEN - 80; i++) {
    char g[16];
    fmt_num(g, sizeof(g), g_pots[i].grams, 1);
    n += snprintf(b + n, BUF_LEN - n, "%s;%s;%d;%s\n", g_pots[i].name, g, g_pots[i].color,
                  g_pots[i].created);
  }
  return storage_write(FILE_POTS, b);
}

int pot_match(float grams, float tol_g, int exclude) {
  int best = -1;
  float best_d = 1e9f;
  for (int i = 0; i < g_pot_count; i++) {
    if (i == exclude) continue;
    float tol = fmaxf(tol_g, g_pots[i].grams * 0.01f);
    float d = fabsf(grams - g_pots[i].grams);
    if (d <= tol && d < best_d) {
      best = i;
      best_d = d;
    }
  }
  return best;
}

// ------------------------------------------------------------
//  Protokoll:  14:32:05;1234,5;g;Notiz
// ------------------------------------------------------------
void log_today(char day[11]) {
  int y, mo, d, h, mi, s;
  if (hal_now(&y, &mo, &d, &h, &mi, &s)) snprintf(day, 11, "%04d-%02d-%02d", y, mo, d);
  else day[0] = 0;
}

// ------------------------------------------------------------
//  Einheiten
// ------------------------------------------------------------
static const char *const UNITS[UNIT_COUNT] = { "g", "kg", "oz", "lb", "ml" };
static const float UNIT_G[UNIT_COUNT] = { 1.0f, 1000.0f, 28.349523f, 453.59237f, 1.0f };  // ml: siehe Dichte
static const int UNIT_DEC[UNIT_COUNT] = { 1, 3, 2, 3, 1 };

// Dichten bei Raumtemperatur (g/ml), gerundete Küchenwerte
typedef struct {
  const char *name;
  float density;
} liquid_t;
static const liquid_t LIQUIDS[] = {
  { "Wasser", 1.00f }, { "Milch", 1.03f },  { "Sahne", 1.00f },    { "Öl", 0.92f },
  { "Olivenöl", 0.91f }, { "Honig", 1.42f }, { "Sirup", 1.30f },   { "Wein", 0.99f },
  { "Bier", 1.01f },   { "Essig", 1.01f },  { "Sojasauce", 1.15f }, { "Brühe", 1.01f },
};
#define LIQUID_N (int)(sizeof(LIQUIDS) / sizeof(LIQUIDS[0]))
static int liquid_ok(int i) { return (i < 0 || i >= LIQUID_N) ? 0 : i; }
int liquid_count() { return LIQUID_N; }
const char *liquid_name(int i) { return LIQUIDS[liquid_ok(i)].name; }
float liquid_density(int i) { return LIQUIDS[liquid_ok(i)].density; }

static int unit_ok(int u) { return (u < 0 || u >= UNIT_COUNT) ? 0 : u; }
const char *unit_name(int u) { return UNITS[unit_ok(u)]; }
int unit_decimals(int u) { return UNIT_DEC[unit_ok(u)]; }
float unit_from_g(float g, int u) {
  if (unit_ok(u) == UNIT_ML) return g / liquid_density(g_set.liquid);
  return g / UNIT_G[unit_ok(u)];
}

int unit_index(const char *name) {
  for (int i = 0; i < UNIT_COUNT; i++)
    if (strcmp(name, UNITS[i]) == 0) return i;
  return 0;
}

void unit_fmt(char *b, int len, float v, int u) {
  int d = unit_decimals(u);
  float half = 0.5f;
  for (int i = 0; i < d; i++) half /= 10.0f;
  if (v > -half && v < half) v = 0.0f;  // kein "-0,000"
  fmt_num(b, len, v, d == 3 ? 3 : d);
}

int log_add(float grams, const char *note) {
  if (!storage_ok()) return 0;
  int y, mo, d, h, mi, s;
  bool valid = hal_now(&y, &mo, &d, &h, &mi, &s);
  char path[64], line[96], g[16];
  if (valid) snprintf(path, sizeof(path), DIR_LOG "/%04d-%02d-%02d.txt", y, mo, d);
  else snprintf(path, sizeof(path), DIR_LOG "/unbekannt.txt");
  int u = g_set.unit;
  unit_fmt(g, sizeof(g), unit_from_g(grams, u), u);
  snprintf(line, sizeof(line), "%02d:%02d:%02d;%s;%s;%s", h, mi, s, g, unit_name(u), note ? note : "");
  if (!storage_append(path, line)) return 0;
  return valid ? 1 : -1;
}

static int cmp_desc(const void *a, const void *b) {
  return -strcmp((const char *)a, (const char *)b);
}

int log_days(char days[][11], int max) {
  static char names[40][STORAGE_NAME_LEN];
  int n = storage_list(DIR_LOG, names, 40);
  int k = 0;
  for (int i = 0; i < n && k < max; i++) {
    // nur Dateien der Form 2026-09-18.txt
    if (strlen(names[i]) == 14 && names[i][4] == '-' && strstr(names[i], ".txt")) {
      memcpy(days[k], names[i], 10);
      days[k][10] = 0;
      k++;
    }
  }
  qsort(days, k, 11, cmp_desc);
  return k;
}

struct log_ctx {
  log_entry_t *tmp;
  int count;
  int max;
};
static void log_line(char *line, void *ctx) {
  log_ctx *c = (log_ctx *)ctx;
  log_entry_t *e = &c->tmp[c->count % c->max];  // Ringpuffer: behält die neuesten
  char f[24];
  field(line, 0, e->time, sizeof(e->time));
  field(line, 1, f, sizeof(f));
  e->value = parse_num(f);
  field(line, 2, e->unit, sizeof(e->unit));
  if (!e->unit[0]) strcpy(e->unit, "g");
  field(line, 3, e->note, sizeof(e->note));
  c->count++;
}

int log_read(const char *day, log_entry_t *out, int max, int *total) {
  *total = 0;
  char path[64];
  snprintf(path, sizeof(path), DIR_LOG "/%s.txt", day);
  char *b = buf();
  int len = b ? storage_read(path, b, BUF_LEN) : -1;
  if (len <= 0) return 0;
  // wurde nur das Ende gelesen, erste (angeschnittene) Zeile verwerfen
  char *start = b;
  if (len >= BUF_LEN - 1) {
    char *nl = strchr(b, '\n');
    if (nl) start = nl + 1;
  }
  static log_entry_t tmp[60];
  if (max > 60) max = 60;
  log_ctx c = { tmp, 0, max };
  each_line(start, log_line, &c);
  *total = c.count;
  int n = c.count < max ? c.count : max;
  for (int i = 0; i < n; i++) out[i] = tmp[(c.count - 1 - i) % max];  // neueste zuerst
  return n;
}

// ------------------------------------------------------------
//  Rezepte:
//    name=Pfannkuchen
//    portionen=2
//    Mehl;250
//    >ruehren;Glatt rühren;2
// ------------------------------------------------------------
static const char *const SI_KEYS[SI_COUNT] = {
  "ruehren", "mixen", "kneten", "braten", "kochen", "backen", "grillen", "schneiden",
  "kuehlen", "ruhen", "mikrowelle", "giessen", "erhitzen", "servieren", "hinweis",
};
static const char *const SI_LABELS[SI_COUNT] = {
  "Rühren", "Mixen", "Kneten", "Braten", "Kochen", "Backen", "Grillen", "Schneiden",
  "Kühlen", "Ruhen", "Mikrowelle", "Gießen", "Erhitzen", "Servieren", "Hinweis",
};
const char *step_icon_key(int icon) { return SI_KEYS[(icon < 0 || icon >= SI_COUNT) ? SI_HINWEIS : icon]; }
const char *step_icon_label(int icon) { return SI_LABELS[(icon < 0 || icon >= SI_COUNT) ? SI_HINWEIS : icon]; }
int step_icon_find(const char *key) {
  while (*key == ' ') key++;
  for (int i = 0; i < SI_COUNT; i++)
    if (!strncasecmp(key, SI_KEYS[i], strlen(SI_KEYS[i])) && !isalpha((unsigned char)key[strlen(SI_KEYS[i])])) return i;
  return SI_HINWEIS;
}

// Text kopieren, ohne ein UTF-8-Zeichen in der Mitte abzuschneiden
static void copy_utf8(char *out, const char *in, int len) {
  int i = 0;
  while (in[i] && i < len - 1) i++;
  if (in[i]) {  // gekürzt: auf Zeichenanfang zurückgehen
    while (i > 0 && ((unsigned char)in[i] & 0xC0) == 0x80) i--;
  }
  memcpy(out, in, i);
  out[i] = 0;
}

static bool s_load_notes = true;  // Cocktails: nur Zutaten

static void recipe_line(char *line, void *ctx) {
  recipe_t *r = (recipe_t *)ctx;
  if (line[0] == '>') {  // Anweisung: >Icon;Text;Minuten
    if (!s_load_notes || r->count >= RECIPE_MAX_ING) return;
    ingredient_t *i = &r->ing[r->count];
    char k[16], t[STEP_TEXT_LEN * 2], m[16];
    field(line + 1, 0, k, sizeof(k));
    field(line + 1, 1, t, sizeof(t));
    field(line + 1, 2, m, sizeof(m));
    memset(i, 0, sizeof(*i));
    i->kind = STEP_NOTE;
    i->icon = (uint8_t)step_icon_find(k);
    copy_utf8(i->text, t, sizeof(i->text));
    float min = parse_num(m);
    if (min > 0) i->secs = (uint16_t)(min * 60.0f + 0.5f > 65000 ? 65000 : min * 60.0f + 0.5f);
    r->count++;
    return;
  }
  if (!strncmp(line, "name=", 5)) {
    strncpy(r->name, line + 5, sizeof(r->name) - 1);
    return;
  }
  if (!strncmp(line, "portionen=", 10)) {
    r->portions = atoi(line + 10);
    if (r->portions < 1) r->portions = 1;
    return;
  }
  if (r->count >= RECIPE_MAX_ING || !strchr(line, ';')) return;
  ingredient_t *i = &r->ing[r->count];
  char f[24];
  memset(i, 0, sizeof(*i));
  field(line, 0, i->name, sizeof(i->name));
  field(line, 1, f, sizeof(f));
  i->grams = parse_num(f);
  i->kind = STEP_WEIGH;
  if (i->name[0] && i->grams > 0) {
    r->count++;
    r->weigh++;
  }
}

bool recipe_load_dir(const char *dir, const char *file, recipe_t *r) {
  memset(r, 0, sizeof(*r));
  r->portions = 1;
  strncpy(r->file, file, sizeof(r->file) - 1);
  char path[80];
  snprintf(path, sizeof(path), "%s/%s", dir, file);
  char *b = buf();
  if (!b || storage_read(path, b, BUF_LEN) < 0) return false;
  s_load_notes = strcmp(dir, DIR_COCKTAILS) != 0;  // Cocktails kennen keine Anweisungen
  each_line(b, recipe_line, r);
  if (!r->name[0]) {  // kein name= -> Dateiname ohne .txt
    strncpy(r->name, file, sizeof(r->name) - 1);
    char *dot = strrchr(r->name, '.');
    if (dot) *dot = 0;
  }
  return r->weigh > 0;
}

int recipes_list_dir(const char *dir, char files[][48], char names[][40], int counts[], int max) {
  static char list[30][STORAGE_NAME_LEN];
  int n = storage_list(dir, list, 30);
  int k = 0;
  static recipe_t r;
  for (int i = 0; i < n && k < max; i++) {
    if (!strstr(list[i], ".txt")) continue;
    if (!recipe_load_dir(dir, list[i], &r)) continue;
    strncpy(files[k], list[i], 47);
    files[k][47] = 0;
    strncpy(names[k], r.name, 39);
    names[k][39] = 0;
    counts[k] = r.weigh;
    k++;
  }
  return k;
}

void recipes_create_example() {
  static char list[4][STORAGE_NAME_LEN];
  if (!storage_ok() || storage_list(DIR_RECIPES, list, 4) > 0) return;
  storage_write(DIR_RECIPES "/pfannkuchen.txt",
                "# Beispielrezept - Zeilen: Zutat;Gramm oder >Icon;Anweisung;Minuten\n"
                "name=Pfannkuchen\n"
                "portionen=2\n"
                "Mehl;250\n"
                "Zucker;20\n"
                "Salz;2\n"
                "Milch;500\n"
                ">ruehren;Mit dem Schneebesen klumpenfrei verrühren\n"
                "Eier;110\n"
                ">ruehren;Eier unterrühren, bis der Teig glatt ist\n"
                ">ruhen;Teig quellen lassen;10\n"
                ">braten;Etwas Butter erhitzen, Teig dünn ausbacken\n"
                ">servieren;Mit Zucker und Zimt oder Apfelmus servieren\n");
}

// ------------------------------------------------------------
//  Porto:  Name;bis Gramm;Preis   z. B. Großbrief;500;1,80
// ------------------------------------------------------------
porto_t g_porto[PORTO_MAX];
int g_porto_count = 0;

void porto_defaults() {
  // Deutsche Post, Stand 2026 (gültig bis 31.12.2026)
  static const porto_t D[] = {
    { "Standardbrief", 20, 95 },
    { "Kompaktbrief", 50, 110 },
    { "Großbrief", 500, 180 },
    { "Maxibrief", 1000, 290 },
  };
  g_porto_count = 4;
  for (int i = 0; i < 4; i++) g_porto[i] = D[i];
}

static void porto_line(char *line, void *ctx) {
  if (g_porto_count >= PORTO_MAX) return;
  porto_t *p = &g_porto[g_porto_count];
  char f[16];
  field(line, 0, p->name, sizeof(p->name));
  field(line, 1, f, sizeof(f));
  p->max_g = atoi(f);
  field(line, 2, f, sizeof(f));
  p->price_ct = (int)lroundf(parse_num(f) * 100.0f);
  if (p->name[0] && p->max_g > 0) g_porto_count++;
}

void porto_load() {
  g_porto_count = 0;
  char *b = buf();
  if (b && storage_read(FILE_PORTO, b, BUF_LEN) > 0) each_line(b, porto_line, NULL);
  if (g_porto_count == 0) porto_defaults();
}

static int porto_cmp(const void *a, const void *b) {
  return ((const porto_t *)a)->max_g - ((const porto_t *)b)->max_g;
}

bool porto_save() {
  qsort(g_porto, g_porto_count, sizeof(porto_t), porto_cmp);
  char *b = buf();
  if (!b) return false;
  int n = snprintf(b, BUF_LEN, "# Name;bis Gramm;Preis in Euro\n");
  for (int i = 0; i < g_porto_count && n < BUF_LEN - 64; i++) {
    n += snprintf(b + n, BUF_LEN - n, "%s;%d;%d,%02d\n", g_porto[i].name, g_porto[i].max_g,
                  g_porto[i].price_ct / 100, g_porto[i].price_ct % 100);
  }
  return storage_write(FILE_PORTO, b);
}

// ------------------------------------------------------------
//  Rezepte speichern / löschen
// ------------------------------------------------------------
// Dateiname aus dem Rezeptnamen: klein, ä -> ae usw., Sonderzeichen -> _
static void slug(const char *name, char *out, int len) {
  int o = 0;
  const unsigned char *p = (const unsigned char *)name;
  while (*p && o < len - 5) {
    if (p[0] == 0xC3 && p[1]) {
      const char *r = NULL;
      switch (p[1]) {
        case 0xA4: case 0x84: r = "ae"; break;
        case 0xB6: case 0x96: r = "oe"; break;
        case 0xBC: case 0x9C: r = "ue"; break;
        case 0x9F: r = "ss"; break;
      }
      if (r) { while (*r && o < len - 5) out[o++] = *r++; }
      p += 2;
      continue;
    }
    char c = (char)*p++;
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[o++] = c;
    else if (o > 0 && out[o - 1] != '_') out[o++] = '_';
  }
  if (o == 0) out[o++] = 'r';
  out[o] = 0;
}

bool recipe_save_dir(const char *dir, const recipe_t *r, char *file_out, int len) {
  char base[32], file[48], path[80];
  slug(r->name, base, sizeof(base));
  snprintf(file, sizeof(file), "%s.txt", base);
  for (int i = 2; i < 100; i++) {  // vorhandene Datei nicht überschreiben
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    if (!storage_exists(path)) break;
    snprintf(file, sizeof(file), "%s_%d.txt", base, i);
  }
  char *b = buf();
  if (!b) return false;
  int n = snprintf(b, BUF_LEN, "name=%s\nportionen=%d\n", r->name, r->portions);
  for (int i = 0; i < r->count && n < BUF_LEN - 128; i++) {
    const ingredient_t *z = &r->ing[i];
    if (z->kind == STEP_NOTE) {  // >Icon;Text;Minuten
      char t[STEP_TEXT_LEN];
      int k = 0;
      for (const char *c = z->text; *c && k < (int)sizeof(t) - 1; c++) t[k++] = (*c == ';' || *c == '\n') ? ',' : *c;
      t[k] = 0;
      n += snprintf(b + n, BUF_LEN - n, ">%s;%s", step_icon_key(z->icon), t);
      if (z->secs) {
        char m[16];
        float min = z->secs / 60.0f;
        fmt_num(m, sizeof(m), min, z->secs % 60 ? 1 : 0);
        n += snprintf(b + n, BUF_LEN - n, ";%s", m);
      }
      n += snprintf(b + n, BUF_LEN - n, "\n");
      continue;
    }
    char g[16];
    fmt_num(g, sizeof(g), z->grams, z->grams == (int)z->grams ? 0 : 1);
    n += snprintf(b + n, BUF_LEN - n, "%s;%s\n", z->name, g);
  }
  if (!storage_write(path, b)) return false;
  if (file_out) snprintf(file_out, len, "%s", file);
  return true;
}

bool recipe_delete_dir(const char *dir, const char *file) {
  char path[80];
  snprintf(path, sizeof(path), "%s/%s", dir, file);
  return storage_remove(path);
}

// ------------------------------------------------------------
//  Leerspulen:  Name;Gewicht
// ------------------------------------------------------------
spool_t g_spools[SPOOL_MAX];
int g_spool_count = 0;

static void spool_line(char *line, void *ctx) {
  if (g_spool_count >= SPOOL_MAX) return;
  spool_t *p = &g_spools[g_spool_count];
  char f[16];
  field(line, 0, p->name, sizeof(p->name));
  field(line, 1, f, sizeof(f));
  p->grams = parse_num(f);
  if (p->name[0] && p->grams > 0) g_spool_count++;
}

void spools_load() {
  g_spool_count = 0;
  char *b = buf();
  if (b && storage_read(FILE_SPOOLS, b, BUF_LEN) > 0) each_line(b, spool_line, NULL);
}

bool spools_save() {
  char *b = buf();
  if (!b) return false;
  int n = snprintf(b, BUF_LEN, "# Name;Gewicht der leeren Spule in g\n");
  for (int i = 0; i < g_spool_count && n < BUF_LEN - 64; i++) {
    char g[16];
    fmt_num(g, sizeof(g), g_spools[i].grams, 1);
    n += snprintf(b + n, BUF_LEN - n, "%s;%s\n", g_spools[i].name, g);
  }
  return storage_write(FILE_SPOOLS, b);
}

// ------------------------------------------------------------
//  Spieler: eine Zeile pro Name
// ------------------------------------------------------------
char g_player_names[PLAYER_MAX][20];
int g_player_count = 3;

static void player_line(char *line, void *ctx) {
  if (g_player_count >= PLAYER_MAX) return;
  strncpy(g_player_names[g_player_count], line, 19);
  g_player_names[g_player_count][19] = 0;
  g_player_count++;
}

void players_load() {
  memset(g_player_names, 0, sizeof(g_player_names));
  g_player_count = 0;
  char *b = buf();
  if (b && storage_read(FILE_PLAYERS, b, BUF_LEN) > 0) each_line(b, player_line, NULL);
  if (g_player_count < 2) g_player_count = 3;
}

bool players_save() {
  char *b = buf();
  if (!b) return false;
  int n = 0;
  b[0] = 0;
  for (int i = 0; i < g_player_count; i++) n += snprintf(b + n, BUF_LEN - n, "%s\n", player_name(i));
  return storage_write(FILE_PLAYERS, b);
}

const char *player_name(int i) {
  static char tmp[PLAYER_MAX][20];
  if (i >= 0 && i < PLAYER_MAX && g_player_names[i][0]) return g_player_names[i];
  int k = (i < 0 || i >= PLAYER_MAX) ? 0 : i;
  snprintf(tmp[k], sizeof(tmp[k]), T("Spieler %d"), i + 1);
  return tmp[k];
}

// ------------------------------------------------------------
//  Ergebnisse der Modi mitschreiben
// ------------------------------------------------------------
static bool s_track_done = false;
static uint32_t s_track_since = 0;

void track_reset() {
  s_track_done = false;
  s_track_since = 0;
}

bool track_update(float net, bool stable, const char *note) {
  if (fabsf(net) < 3.0f) {  // leer -> bereit für das nächste Ergebnis
    track_reset();
    return false;
  }
  if (s_track_done || net < 5.0f) return false;
  if (!stable) {
    s_track_since = 0;
    return false;
  }
  uint32_t now = hal_millis();
  if (s_track_since == 0) s_track_since = now;
  if (now - s_track_since < 1500) return false;
  s_track_done = true;
  return log_add(net, note) != 0;
}

// ------------------------------------------------------------
//  Bequeme Fassungen für den Rezepte-Ordner
// ------------------------------------------------------------
bool recipe_load(const char *file, recipe_t *r) { return recipe_load_dir(DIR_RECIPES, file, r); }
int recipes_list(char files[][48], char names[][40], int counts[], int max) {
  return recipes_list_dir(DIR_RECIPES, files, names, counts, max);
}
bool recipe_save(const recipe_t *r, char *file_out, int len) { return recipe_save_dir(DIR_RECIPES, r, file_out, len); }
bool recipe_delete(const char *file) { return recipe_delete_dir(DIR_RECIPES, file); }

// ------------------------------------------------------------
//  Cocktails: Rezepte in Millilitern. Gewogen wird in Gramm,
//  deshalb rechnet die Waage über die Dichte der Zutat um.
// ------------------------------------------------------------
typedef struct {
  const char *key;   // Teil des Zutatennamens, klein geschrieben
  float density;     // g/ml
} density_t;

static const density_t DENSITIES[] = {
  { "sirup", 1.28f },   { "syrup", 1.28f },    { "zuckersirup", 1.30f }, { "honig", 1.42f },
  { "likör", 1.08f },   { "likor", 1.08f },    { "baileys", 1.05f },     { "sahne", 1.01f },
  { "milch", 1.03f },   { "saft", 1.05f },     { "juice", 1.05f },       { "sirop", 1.28f },
  { "limette", 1.03f }, { "zitrone", 1.03f },  { "orange", 1.05f },      { "ananas", 1.05f },
  { "cola", 1.04f },    { "tonic", 1.04f },    { "ginger", 1.04f },      { "soda", 1.00f },
  { "wasser", 1.00f },  { "eis", 0.92f },      { "wodka", 0.95f },       { "vodka", 0.95f },
  { "gin", 0.94f },     { "rum", 0.94f },      { "tequila", 0.94f },     { "whisk", 0.94f },
  { "cachaca", 0.94f }, { "cachaça", 0.94f },  { "wermut", 0.99f },      { "vermouth", 0.99f },
  { "aperol", 1.06f },  { "campari", 1.06f },  { "pfirsich", 1.08f }, { "triple sec", 1.04f },
  { "grenadine", 1.30f }, { "holunder", 1.28f }, { "kokos", 1.03f },   { "cranberry", 1.05f },  { "sekt", 0.99f },        { "prosecco", 0.99f },
  { "wein", 0.99f },    { "espresso", 1.00f },
  // englische Zutatennamen (Sprache English)
  { "liqueur", 1.08f }, { "honey", 1.42f },    { "cream", 1.01f },       { "milk", 1.03f },
  { "lime", 1.03f },    { "lemon", 1.03f },    { "pineapple", 1.05f },   { "passion", 1.05f },
  { "elder", 1.28f },   { "coconut", 1.03f },  { "peach", 1.08f },       { "sparkling", 0.99f },
  { "wine", 0.99f },    { "water", 1.00f },    { "ice", 0.92f },
};

static char lower_buf[40];
static const char *to_lower(const char *s) {
  int i = 0;
  for (; s[i] && i < (int)sizeof(lower_buf) - 1; i++) {
    char c = s[i];
    lower_buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
  }
  lower_buf[i] = 0;
  return lower_buf;
}

float ingredient_density(const char *name) {
  const char *low = to_lower(name);
  for (unsigned i = 0; i < sizeof(DENSITIES) / sizeof(DENSITIES[0]); i++)
    if (strstr(low, DENSITIES[i].key)) return DENSITIES[i].density;
  return 1.0f;  // unbekannt: wie Wasser
}

void cocktails_create_examples() {
  char list[4][STORAGE_NAME_LEN];
  if (!storage_ok() || storage_list(DIR_COCKTAILS, list, 4) > 0) return;
  storage_write(DIR_COCKTAILS "/gin_tonic.txt",
                "name=Gin Tonic\nportionen=1\nGin;50\nTonic Water;150\nLimettensaft;10\n");
  storage_write(DIR_COCKTAILS "/cuba_libre.txt",
                "name=Cuba Libre\nportionen=1\nRum braun;50\nCola;120\nLimettensaft;10\n");
  storage_write(DIR_COCKTAILS "/caipirinha.txt",
                "name=Caipirinha\nportionen=1\nCachaca;60\nLimettensaft;30\nZuckersirup;20\n");
  storage_write(DIR_COCKTAILS "/aperol_spritz.txt",
                "name=Aperol Spritz\nportionen=1\nAperol;60\nProsecco;90\nSoda;30\n");
}

// ------------------------------------------------------------
//  Spieler: suchen und anlegen
// ------------------------------------------------------------
bool g_player_in[PLAYER_MAX];

int players_pick_count() {
  int n = 0;
  for (int i = 0; i < g_player_count; i++)
    if (g_player_in[i]) n++;
  return n;
}

int players_pick_index(int nth) {
  for (int i = 0; i < g_player_count; i++)
    if (g_player_in[i] && nth-- == 0) return i;
  return 0;
}

void players_pick_default() {
  if (players_pick_count() >= 2) return;  // Auswahl steht schon
  for (int i = 0; i < g_player_count; i++) g_player_in[i] = (i < 3);
}

int player_find(const char *name) {
  for (int i = 0; i < g_player_count; i++)
    if (strcasecmp(g_player_names[i], name) == 0) return i;
  return -1;
}

bool player_add(const char *name) {
  if (!name || !name[0]) return false;
  if (player_find(name) >= 0) return true;
  if (g_player_count >= PLAYER_MAX) return false;
  strncpy(g_player_names[g_player_count], name, sizeof(g_player_names[0]) - 1);
  g_player_names[g_player_count][sizeof(g_player_names[0]) - 1] = 0;
  g_player_count++;
  return players_save();
}

// ------------------------------------------------------------
//  Bestenliste: alle Spiele in einer Datei
// ------------------------------------------------------------
bool score_add(const char *game, const char *name, float value, const char *unit) {
  char day[11];
  log_today(day);
  char line[80], val[12];
  fmt_num(val, sizeof(val), value, 1);
  snprintf(line, sizeof(line), "%s;%s;%s;%s;%s", day[0] ? day : "?", game, name, val, unit);
  return storage_append(FILE_SCORES, line);
}

int scores_load(score_t *out, int max) {
  char *b = buf();
  if (!b) return 0;
  int n = storage_read(FILE_SCORES, b, BUF_LEN);
  if (n <= 0) return 0;
  int count = 0;
  char *line = strtok(b, "\n");
  while (line && count < max) {
    score_t s = {};
    char val[16] = "";
    // Datum;Spiel;Name;Wert;Einheit
    if (sscanf(line, "%10[^;];%11[^;];%19[^;];%15[^;];%3s", s.day, s.game, s.name, val, s.unit) >= 4) {
      s.value = parse_num(val);
      out[count++] = s;
    }
    line = strtok(NULL, "\n");
  }
  // neueste zuerst
  for (int i = 0; i < count / 2; i++) {
    score_t t = out[i];
    out[i] = out[count - 1 - i];
    out[count - 1 - i] = t;
  }
  return count;
}
