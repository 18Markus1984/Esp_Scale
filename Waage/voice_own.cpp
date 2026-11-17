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

// Deutsch: Reihenfolge = Liste an der Waage und im Browser
static const own_slot_t SLOTS_DE[] = {
  { "tara", "Tara", 0, 6 },
  { "gespeichert", "Gespeichert", 0, 6 },
  { "ziel_erreicht", "Ziel erreicht", 0, 6 },
  { "ueberlast", "Überlast", 0, 6 },
  { "topf_erkannt", "Topf erkannt", 0, 6 },
  { "fertig", "Fertig", 0, 6 },
  { "waage_leer", "Waage leer", 0, 6 },
  { "ja", "Ja?", 1, 3 },
  { "nicht_verst", "Das habe ich nicht verstanden, sag Hilfe", 1, 6 },
  { "hilfe", "Sag Hallo Waage, dann zum Beispiel Tara, wie viel, speichern …", 1, 20 },
  { "bereit", "Waage bereit", 1, 5 },
  { "weiter", "Weiter", 1, 4 },
  { "zurueck", "Zurück zur Startseite", 1, 5 },
  { "geht_nicht", "Das geht hier gerade nicht", 1, 5 },
  { "ziel_beendet", "Zielwiegen beendet", 1, 5 },
  { "einfuellen", "Jetzt einfüllen", 1, 4 },
  { "akku_leer", "Akku fast leer, bitte laden", 1, 6 },
  { "timer_ab", "Der Timer ist abgelaufen", 1, 6 },
  { "zeit_unbek", "Die Uhrzeit kenne ich noch nicht", 1, 6 },
  { "schaltet_aus", "Die Waage schaltet sich gleich aus", 1, 6 },
  { "laedt", "wird geladen", 1, 4 },
  { "ziel", "Ziel", 2, 3 }, { "akku", "Akku", 2, 3 }, { "prozent", "Prozent", 2, 3 },
  { "uhr", "Uhr", 2, 3 }, { "lautstaerke", "Lautstärke", 2, 3 }, { "noch", "noch", 2, 3 },
  { "zu_viel", "zu viel", 2, 3 },
  { "null", "null", 3, 3 }, { "eins", "eins", 3, 3 }, { "ein", "ein (wie in einundzwanzig)", 3, 3 },
  { "zwei", "zwei", 3, 3 }, { "drei", "drei", 3, 3 }, { "vier", "vier", 3, 3 }, { "fuenf", "fünf", 3, 3 },
  { "sechs", "sechs", 3, 3 }, { "sieben", "sieben", 3, 3 }, { "acht", "acht", 3, 3 }, { "neun", "neun", 3, 3 },
  { "zehn", "zehn", 3, 3 }, { "elf", "elf", 3, 3 }, { "zwoelf", "zwölf", 3, 3 }, { "dreizehn", "dreizehn", 3, 3 },
  { "vierzehn", "vierzehn", 3, 3 }, { "fuenfzehn", "fünfzehn", 3, 3 }, { "sechzehn", "sechzehn", 3, 3 },
  { "siebzehn", "siebzehn", 3, 3 }, { "achtzehn", "achtzehn", 3, 3 }, { "neunzehn", "neunzehn", 3, 3 },
  { "zwanzig", "zwanzig", 3, 3 }, { "dreissig", "dreißig", 3, 3 }, { "vierzig", "vierzig", 3, 3 },
  { "fuenfzig", "fünfzig", 3, 3 }, { "sechzig", "sechzig", 3, 3 }, { "siebzig", "siebzig", 3, 3 },
  { "achtzig", "achtzig", 3, 3 }, { "neunzig", "neunzig", 3, 3 }, { "hundert", "hundert", 3, 3 },
  { "tausend", "tausend", 3, 3 }, { "und", "und", 3, 3 }, { "komma", "Komma", 3, 3 }, { "minus", "minus", 3, 3 },
  { "gramm", "Gramm", 2, 3 }, { "kilogramm", "Kilogramm", 2, 3 }, { "milliliter", "Milliliter", 2, 3 },
  { "unze", "Unzen", 2, 3 }, { "pfund", "Pfund", 2, 3 }, { "stueck", "Stück", 2, 3 }, { "meter", "Meter", 2, 3 },
};

static const own_slot_t SLOTS_EN[] = {
  { "tare", "Tare", 0, 6 },
  { "saved", "Saved", 0, 6 },
  { "goal_reached", "Target reached", 0, 6 },
  { "overload", "Overload", 0, 6 },
  { "pot_detected", "Pot detected", 0, 6 },
  { "done", "Done", 0, 6 },
  { "scale_empty", "Scale empty", 0, 6 },
  { "yes", "Yes?", 1, 3 },
  { "not_underst", "I did not understand that, say help", 1, 6 },
  { "help", "Say Hallo Waage, then for example tare, how much, save …", 1, 20 },
  { "ready", "Scale ready", 1, 5 },
  { "next", "Next", 1, 4 },
  { "back", "Back to the start page", 1, 5 },
  { "cannot", "That is not possible right now", 1, 5 },
  { "target_off", "Target weighing stopped", 1, 5 },
  { "fill_now", "Fill in now", 1, 4 },
  { "batt_low", "Battery low, please charge", 1, 6 },
  { "timer_done", "The timer has finished", 1, 6 },
  { "time_unknown", "I do not know the time yet", 1, 6 },
  { "power_off", "The scale will switch off soon", 1, 6 },
  { "charging", "charging", 1, 4 },
  { "target", "Target", 2, 3 }, { "battery", "Battery", 2, 3 }, { "percent", "percent", 2, 3 },
  { "oclock", "o'clock", 2, 3 }, { "volume", "Volume", 2, 3 }, { "to_go", "to go", 2, 3 },
  { "too_much", "too much", 2, 3 },
  { "zero", "zero", 3, 3 }, { "one", "one", 3, 3 }, { "two", "two", 3, 3 }, { "three", "three", 3, 3 },
  { "four", "four", 3, 3 }, { "five", "five", 3, 3 }, { "six", "six", 3, 3 }, { "seven", "seven", 3, 3 },
  { "eight", "eight", 3, 3 }, { "nine", "nine", 3, 3 }, { "ten", "ten", 3, 3 }, { "eleven", "eleven", 3, 3 },
  { "twelve", "twelve", 3, 3 }, { "thirteen", "thirteen", 3, 3 }, { "fourteen", "fourteen", 3, 3 },
  { "fifteen", "fifteen", 3, 3 }, { "sixteen", "sixteen", 3, 3 }, { "seventeen", "seventeen", 3, 3 },
  { "eighteen", "eighteen", 3, 3 }, { "nineteen", "nineteen", 3, 3 }, { "twenty", "twenty", 3, 3 },
  { "thirty", "thirty", 3, 3 }, { "forty", "forty", 3, 3 }, { "fifty", "fifty", 3, 3 }, { "sixty", "sixty", 3, 3 },
  { "seventy", "seventy", 3, 3 }, { "eighty", "eighty", 3, 3 }, { "ninety", "ninety", 3, 3 },
  { "hundred", "hundred", 3, 3 }, { "thousand", "thousand", 3, 3 }, { "and", "and (one hundred and five)", 3, 3 },
  { "point", "point", 3, 3 },
  { "minus", "minus", 3, 3 }, { "grams", "grams", 2, 3 }, { "kilograms", "kilograms", 2, 3 },
  { "milliliters", "milliliters", 2, 3 }, { "ounces", "ounces", 2, 3 }, { "pounds", "pounds", 2, 3 },
  { "pieces", "pieces", 2, 3 }, { "meters", "meters", 2, 3 },
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
  static const char *const N[OWN_GROUPS] = { "Ansagen", "Sprachsteuerung", "Bausteine und Einheiten", "Zahlen" };
  return N[g < 0 || g >= OWN_GROUPS ? 0 : g];
}

void own_path(char *buf, int len, const char *file) {
  snprintf(buf, len, VOICE_DIR "/%s/%s.wav", own_pack(), file);
}

bool own_exists() {
  char p[96];
  own_path(p, sizeof(p), en() ? "grams" : "gramm");  // daran erkennt die Waage ein Paket
  return storage_exists(p);
}

bool own_create(const char *from) {
  if (!storage_ok()) return false;
  char dir[64], src[96], dst[96];
  snprintf(dir, sizeof(dir), VOICE_DIR "/%s", own_pack());
  storage_mkdir(VOICE_DIR);
  storage_mkdir(dir);
  const char *base = (from && from[0]) ? from : g_set.voice;
  const char *extra = en() ? "_Zusatz-EN" : "_Zusatz";
  const own_slot_t *sl;
  int n = own_slots(&sl), ok = 0;
  for (int i = 0; i < n; i++) {
    own_path(dst, sizeof(dst), sl[i].file);
    if (storage_exists(dst)) {  // schon da (z. B. eigene Aufnahme): nicht überschreiben
      ok++;
      continue;
    }
    snprintf(src, sizeof(src), VOICE_DIR "/%s/%s.wav", base, sl[i].file);
    if (!base[0] || strcmp(base, own_pack()) == 0 || !storage_copy(src, dst)) {
      snprintf(src, sizeof(src), VOICE_DIR "/%s/%s.wav", extra, sl[i].file);
      if (!storage_copy(src, dst)) continue;
    }
    ok++;
  }
  printf("Eigene Stimme: %d von %d Dateien aus %s\r\n", ok, n, base);
  return own_exists();
}

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
