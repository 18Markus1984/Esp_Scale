// ============================================================
//  Eigene Stimme – Verwaltung (siehe voice_own.h)
// ============================================================
#include "voice_own.h"
#include "storage.h"
#include "settings.h"
#include "i18n.h"
#include <string.h>
#include <stdio.h>

#define VOICE_DIR "/Waage/Stimme"
#define MINE_FILE "_eigen.txt"
#define BASE_FILE "_basis.txt"

// Deutsch: Reihenfolge = Liste an der Waage und im Browser
static const own_slot_t SLOTS_DE[] = {
  { "tara", "Tara", 0, 6, "nach Tara (Waage auf null)" },
  { "gespeichert", "Gespeichert", 0, 6, "wenn ein Gewicht gespeichert wurde" },
  { "ziel_erreicht", "Ziel erreicht", 0, 6, "wenn das Zielgewicht erreicht ist" },
  { "ueberlast", "Überlast", 0, 6, "wenn zu viel auf der Waage liegt" },
  { "topf_erkannt", "Topf erkannt", 0, 6, "wenn sie einen gespeicherten Topf erkennt" },
  { "fertig", "Fertig", 0, 6, "wenn ein Schritt oder Rezept fertig ist" },
  { "waage_leer", "Waage leer", 0, 6, "wenn die Waage wieder leer ist" },
  { "ja", "Ja?", 1, 3, "nach „Hallo Waage“: sie hört jetzt zu" },
  { "nicht_verst", "Das habe ich nicht verstanden, sag Hilfe", 1, 6, "wenn sie einen Befehl nicht verstanden hat" },
  { "hilfe", "Sag Hallo Waage, dann zum Beispiel Tara, wie viel, speichern …", 1, 20, "auf „Hilfe“: erklärt, was man sagen kann" },
  { "bereit", "Waage bereit", 1, 5, "beim Einschalten" },
  { "weiter", "Weiter", 1, 4, "auf „Weiter“" },
  { "zurueck", "Zurück zur Startseite", 1, 5, "auf „Zurück“" },
  { "geht_nicht", "Das geht hier gerade nicht", 1, 5, "wenn ein Befehl gerade nicht geht" },
  { "ziel_beendet", "Zielwiegen beendet", 1, 5, "auf „Stopp“ beim Zielwiegen" },
  { "einfuellen", "Jetzt einfüllen", 1, 4, "nach einem Ziel: „Ziel 250 Gramm …“" },
  { "akku_leer", "Akku fast leer, bitte laden", 1, 6, "wenn der Akku fast leer ist" },
  { "timer_ab", "Der Timer ist abgelaufen", 1, 6, "wenn ein Timer abgelaufen ist" },
  { "zeit_unbek", "Die Uhrzeit kenne ich noch nicht", 1, 6, "auf „Uhrzeit“, wenn sie die Zeit nicht kennt" },
  { "schaltet_aus", "Die Waage schaltet sich gleich aus", 1, 6, "kurz bevor sie sich ausschaltet" },
  { "laedt", "wird geladen", 1, 4, "nach dem Akkustand, wenn sie lädt: „Akku 80 Prozent …“" },
  { "ziel", "Ziel", 2, 3, "Baustein: „Ziel“ 250 Gramm" }, { "akku", "Akku", 2, 3, "Baustein: „Akku“ 80 Prozent" }, { "prozent", "Prozent", 2, 3, "Baustein: Akku 80 „Prozent“" },
  { "uhr", "Uhr", 2, 3, "Baustein: 14 „Uhr“ 5" }, { "lautstaerke", "Lautstärke", 2, 3, "Baustein: „Lautstärke“ 60 Prozent" }, { "noch", "noch", 2, 3, "Baustein: „noch“ 120 Gramm" },
  { "zu_viel", "zu viel", 2, 3, "Baustein: 20 Gramm „zu viel“" },
  { "null", "null", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "eins", "eins", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "ein", "ein (wie in einundzwanzig)", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "zwei", "zwei", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "drei", "drei", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "vier", "vier", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "fuenf", "fünf", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "sechs", "sechs", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "sieben", "sieben", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "acht", "acht", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "neun", "neun", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "zehn", "zehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "elf", "elf", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "zwoelf", "zwölf", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "dreizehn", "dreizehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "vierzehn", "vierzehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "fuenfzehn", "fünfzehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "sechzehn", "sechzehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "siebzehn", "siebzehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "achtzehn", "achtzehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "neunzehn", "neunzehn", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "zwanzig", "zwanzig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "dreissig", "dreißig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "vierzig", "vierzig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "fuenfzig", "fünfzig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "sechzig", "sechzig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "siebzig", "siebzig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "achtzig", "achtzig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "neunzig", "neunzig", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "hundert", "hundert", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "tausend", "tausend", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "und", "und", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "komma", "Komma", 3, 3, "Zahl, wird mit anderen zusammengesetzt" }, { "minus", "minus", 3, 3, "Zahl, wird mit anderen zusammengesetzt" },
  { "gramm", "Gramm", 2, 3, "Einheit nach der Zahl" }, { "kilogramm", "Kilogramm", 2, 3, "Einheit nach der Zahl" }, { "milliliter", "Milliliter", 2, 3, "Einheit nach der Zahl" },
  { "unze", "Unzen", 2, 3, "Einheit nach der Zahl" }, { "pfund", "Pfund", 2, 3, "Einheit nach der Zahl" }, { "stueck", "Stück", 2, 3, "Einheit nach der Zahl" }, { "meter", "Meter", 2, 3, "Einheit nach der Zahl" },
};

static const own_slot_t SLOTS_EN[] = {
  { "tare", "Tare", 0, 6, "after tare (scale at zero)" },
  { "saved", "Saved", 0, 6, "when a weight was saved" },
  { "goal_reached", "Target reached", 0, 6, "when the target is reached" },
  { "overload", "Overload", 0, 6, "when too much is on the scale" },
  { "pot_detected", "Pot detected", 0, 6, "when a saved pot is recognised" },
  { "done", "Done", 0, 6, "when a step or recipe is done" },
  { "scale_empty", "Scale empty", 0, 6, "when the scale is empty again" },
  { "yes", "Yes?", 1, 3, "after “Hallo Waage”: it is listening" },
  { "not_underst", "I did not understand that, say help", 1, 6, "when it did not understand a command" },
  { "help", "Say Hallo Waage, then for example tare, how much, save …", 1, 20, "on “help”: explains the commands" },
  { "ready", "Scale ready", 1, 5, "when switching on" },
  { "next", "Next", 1, 4, "on “next”" },
  { "back", "Back to the start page", 1, 5, "on “back”" },
  { "cannot", "That is not possible right now", 1, 5, "when a command is not possible right now" },
  { "target_off", "Target weighing stopped", 1, 5, "on “stop” while target weighing" },
  { "fill_now", "Fill in now", 1, 4, "after a target: “Target 250 grams …”" },
  { "batt_low", "Battery low, please charge", 1, 6, "when the battery is almost empty" },
  { "timer_done", "The timer has finished", 1, 6, "when a timer has finished" },
  { "time_unknown", "I do not know the time yet", 1, 6, "on “time”, when it does not know the time" },
  { "power_off", "The scale will switch off soon", 1, 6, "shortly before it switches off" },
  { "charging", "charging", 1, 4, "after the battery level while charging" },
  { "target", "Target", 2, 3, "block: “Target” 250 grams" }, { "battery", "Battery", 2, 3, "block: “Battery” 80 percent" }, { "percent", "percent", 2, 3, "block: battery 80 “percent”" },
  { "oclock", "o'clock", 2, 3, "block: 2 “o'clock”" }, { "volume", "Volume", 2, 3, "block: “Volume” 60 percent" }, { "to_go", "to go", 2, 3, "block: 120 grams “to go”" },
  { "too_much", "too much", 2, 3, "block: 20 grams “too much”" },
  { "zero", "zero", 3, 3, "number, combined with others" }, { "one", "one", 3, 3, "number, combined with others" }, { "two", "two", 3, 3, "number, combined with others" }, { "three", "three", 3, 3, "number, combined with others" },
  { "four", "four", 3, 3, "number, combined with others" }, { "five", "five", 3, 3, "number, combined with others" }, { "six", "six", 3, 3, "number, combined with others" }, { "seven", "seven", 3, 3, "number, combined with others" },
  { "eight", "eight", 3, 3, "number, combined with others" }, { "nine", "nine", 3, 3, "number, combined with others" }, { "ten", "ten", 3, 3, "number, combined with others" }, { "eleven", "eleven", 3, 3, "number, combined with others" },
  { "twelve", "twelve", 3, 3, "number, combined with others" }, { "thirteen", "thirteen", 3, 3, "number, combined with others" }, { "fourteen", "fourteen", 3, 3, "number, combined with others" },
  { "fifteen", "fifteen", 3, 3, "number, combined with others" }, { "sixteen", "sixteen", 3, 3, "number, combined with others" }, { "seventeen", "seventeen", 3, 3, "number, combined with others" },
  { "eighteen", "eighteen", 3, 3, "number, combined with others" }, { "nineteen", "nineteen", 3, 3, "number, combined with others" }, { "twenty", "twenty", 3, 3, "number, combined with others" },
  { "thirty", "thirty", 3, 3, "number, combined with others" }, { "forty", "forty", 3, 3, "number, combined with others" }, { "fifty", "fifty", 3, 3, "number, combined with others" }, { "sixty", "sixty", 3, 3, "number, combined with others" },
  { "seventy", "seventy", 3, 3, "number, combined with others" }, { "eighty", "eighty", 3, 3, "number, combined with others" }, { "ninety", "ninety", 3, 3, "number, combined with others" },
  { "hundred", "hundred", 3, 3, "number, combined with others" }, { "thousand", "thousand", 3, 3, "number, combined with others" }, { "and", "and (one hundred and five)", 3, 3, "number, combined with others" },
  { "point", "point", 3, 3, "number, combined with others" },
  { "minus", "minus", 3, 3, "number, combined with others" }, { "grams", "grams", 2, 3, "unit after the number" }, { "kilograms", "kilograms", 2, 3, "unit after the number" },
  { "milliliters", "milliliters", 2, 3, "unit after the number" }, { "ounces", "ounces", 2, 3, "unit after the number" }, { "pounds", "pounds", 2, 3, "unit after the number" },
  { "pieces", "pieces", 2, 3, "unit after the number" }, { "meters", "meters", 2, 3, "unit after the number" },
};

static bool en() { return g_set.lang == LANG_EN; }

const char *own_pack() { return en() ? "Eigene Stimme EN" : "Eigene Stimme"; }

int own_slots(const own_slot_t **out) {
  if (en()) {
    *out = SLOTS_EN;
    return sizeof(SLOTS_EN) / sizeof(SLOTS_EN[0]);
  }
  *out = SLOTS_DE;
  return sizeof(SLOTS_DE) / sizeof(SLOTS_DE[0]);
}

const char *own_group_name(int g) {
  static const char *const N[OWN_GROUPS] = { "Ansagen", "Sprachsteuerung", "Wörter und Einheiten", "Zahlen" };
  return N[g < 0 || g >= OWN_GROUPS ? 0 : g];
}

void own_path(char *buf, int len, const char *file) {
  snprintf(buf, len, VOICE_DIR "/%s/%s.wav", own_pack(), file);
}

// Angelegt = Ordner mit _basis.txt (Name der Vorlage). Es wird nichts kopiert: was noch nicht
// aufgenommen ist, spielt sound.cpp aus der Vorlage (früher: ~70 Dateien kopieren, dauerte so
// lange, dass der Wächter die Waage neu startete).
static char s_base[24] = "";
static char s_base_lang = 0;

static void base_path(char *p, int len) { snprintf(p, len, VOICE_DIR "/%s/" BASE_FILE, own_pack()); }

bool own_exists() {
  char p[96];
  base_path(p, sizeof(p));
  if (storage_exists(p)) return true;
  // mit der ersten Version angelegt (alles kopiert, ohne _basis.txt)
  snprintf(p, sizeof(p), VOICE_DIR "/%s/%s.wav", own_pack(), en() ? "grams" : "gramm");
  return storage_exists(p);
}

const char *own_base() {
  char lang = en() ? 'e' : 'd';
  if (s_base_lang != lang) {
    s_base_lang = lang;
    s_base[0] = 0;
    char p[96];
    base_path(p, sizeof(p));
    int n = storage_read_bin(p, s_base, sizeof(s_base) - 1);
    if (n < 0) n = 0;
    s_base[n] = 0;
    char *e = strpbrk(s_base, "\r\n");
    if (e) *e = 0;
  }
  return s_base;
}

bool own_create(const char *from) {
  if (!storage_ok()) return false;
  char dir[64], p[96];
  snprintf(dir, sizeof(dir), VOICE_DIR "/%s", own_pack());
  storage_mkdir(VOICE_DIR);
  storage_mkdir(dir);
  const char *base = (from && from[0]) ? from : g_set.voice;
  if (!base[0] || strcmp(base, own_pack()) == 0) base = "";
  base_path(p, sizeof(p));
  bool ok = storage_write(p, base);
  s_base_lang = 0;  // neu lesen
  printf("Eigene Stimme angelegt, Vorlage \"%s\"\r\n", base);
  return ok;
}

bool own_is_pack(const char *pack) { return pack && strcmp(pack, own_pack()) == 0; }

// _eigen.txt: eine Zeile pro selbst aufgenommener Datei
static char s_mine[1200];
static bool s_mine_loaded = false;
static char s_mine_lang = 0;

static void mine_load() {
  char lang = en() ? 'e' : 'd';
  if (s_mine_loaded && s_mine_lang == lang) return;
  char p[96];
  snprintf(p, sizeof(p), VOICE_DIR "/%s/" MINE_FILE, own_pack());
  int n = storage_read_bin(p, s_mine + 1, sizeof(s_mine) - 3);
  if (n < 0) n = 0;
  s_mine[0] = '\n';
  s_mine[n + 1] = '\n';
  s_mine[n + 2] = 0;
  s_mine_loaded = true;
  s_mine_lang = lang;
}

bool own_is_mine(const char *file) {
  mine_load();
  char k[24];
  snprintf(k, sizeof(k), "\n%s\n", file);
  return strstr(s_mine, k) != NULL;
}

void own_mark(const char *file) {
  if (own_is_mine(file)) return;
  char p[96];
  snprintf(p, sizeof(p), VOICE_DIR "/%s/" MINE_FILE, own_pack());
  storage_append(p, file);
  s_mine_loaded = false;
}

int own_count_mine() {
  const own_slot_t *sl;
  int n = own_slots(&sl), c = 0;
  for (int i = 0; i < n; i++)
    if (own_is_mine(sl[i].file)) c++;
  return c;
}
