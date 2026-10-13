// ============================================================
//  Sprachbefehle
//
//  Standard (USE_VOICE 0, unten ab "eigene Schlüsselwort-Erkennung"):
//  eigenes kleines Netz (kws.cpp), Modell von der SD-Karte, deutsches
//  Weckwort "Hallo Waage". Trainiert wird es mit dem Werkzeug im Ordner
//  Sprachmodell; Aufnahmen dafür macht die Waage selbst
//  (Setup -> Ton -> Wörter aufnehmen).
//
//  Alt (USE_VOICE 1): ESP-SR, nur ESP32-S3, englische Befehle:
//
//  Wofür sinnvoll: genau dann, wenn beide Hände voll oder klebrig
//  sind. Deshalb nur wenige, kurze Befehle rund ums Wiegen.
//
//  Ablauf: Weckwort "Hi ESP" sagen, danach innerhalb weniger
//  Sekunden einen Befehl:
//    "tare the scale"   -> Tara
//    "save the weight"  -> ins Protokoll speichern
//    "next step"        -> Weiter (Rezept, Cocktail, Spiele)
//    "read the weight"  -> Gewicht ansagen (Sprachausgabe)
//    "go back"          -> zurück zur Wiegeseite
//    "stop"             -> laufenden Modus beenden
//
//  Die Befehle sind englisch, weil ESP-SR (MultiNet) nur Englisch
//  und Chinesisch kann. Die Lautschrift daneben stammt aus dem
//  Werkzeug multinet_g2p.py von Espressif.
//
//  Einschalten:
//   1. In config.h  USE_VOICE 1
//   2. In der Arduino-IDE das Partitionsschema "ESP SR 16M" wählen
//      (die Modelle brauchen eine eigene Partition)
//   3. Einmal hochladen; beim ersten Start werden die Modelle geschrieben
//  Vorher mit System -> Mikrofon prüfen, ob das Mikrofon im Gehäuse
//  überhaupt genug hört.
// ============================================================
#include "voice.h"
#include "config.h"

#if USE_VOICE && defined(ARDUINO)
#include <Arduino.h>
#include <ESP_SR.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include "mic.h"
#include "ui.h"
#include "scale.h"
#include "sound.h"
#include "settings.h"
#include "data.h"
#include "web.h"
#include "i18n.h"

extern I2SClass &mic_i2s();

enum {
  CMD_TARE,
  CMD_SAVE,
  CMD_NEXT,
  CMD_READ,
  CMD_BACK,
  CMD_STOP,
};

static const sr_cmd_t VOICE_CMDS[] = {
  { CMD_TARE, "tare the scale", "TfR jc SKdL" },
  { CMD_SAVE, "save the weight", "SdV jc WdT" },
  { CMD_NEXT, "next step", "NfKST STfP" },
  { CMD_READ, "read the weight", "RfD jc WdT" },
  { CMD_BACK, "go back", "Gb BaK" },
  { CMD_STOP, "stop", "STnP" },
};

static volatile int s_pending = -1;  // vom SR-Task gesetzt, im UI-Takt ausgeführt
static volatile bool s_awake = false;
static bool s_running = false;
static const char *s_last = "";
static volatile unsigned long s_last_ms = 0;

// Der Rückruf läuft in einem eigenen Task: hier nur merken, nichts zeichnen
static void sr_event(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_WAKEWORD:
      s_awake = true;
      break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      ESP_SR.setMode(SR_MODE_COMMAND);
      break;
    case SR_EVENT_TIMEOUT:
      s_awake = false;
      ESP_SR.setMode(SR_MODE_WAKEWORD);
      break;
    case SR_EVENT_COMMAND:
      s_pending = command_id;
      s_last = VOICE_CMDS[phrase_id].str;
      s_last_ms = millis();
      ESP_SR.setMode(SR_MODE_COMMAND);
      break;
    default:
      break;
  }
}

// ESP-SR legt seine Tasks mit hoher Priorität an, den Erkenner sogar auf Kern 1,
// also dort, wo auch die Oberfläche gezeichnet wird ("SR Detect Task", Priorität 5,
// "SR Handler Task" sogar maximale Priorität). Dadurch ruckelt die Anzeige.
// Nach dem Start setzen wir die Prioritäten herunter: Die Erkennung wird dadurch
// minimal träger, die Bedienung bleibt aber flüssig.
static void lower_sr_priority() {
#if defined(INCLUDE_xTaskGetHandle) && INCLUDE_xTaskGetHandle
  struct {
    const char *name;
    UBaseType_t prio;
  } tasks[] = { { "SR Detect Task", 2 }, { "SR Handler Task", 3 }, { "SR Feed Task", 4 } };
  for (auto &t : tasks) {
    TaskHandle_t h = xTaskGetHandle(t.name);
    if (h) {
      vTaskPrioritySet(h, t.prio);
      printf("Sprache: %s auf Priorität %u gesetzt\r\n", t.name, (unsigned)t.prio);
    }
  }
#endif
}

// Der Start von ESP-SR lädt die Sprachmodelle aus dem Flash und braucht
// dafür ein bis zwei Sekunden. Das darf nicht im UI-Takt passieren, sonst
// steht die Oberfläche still. Deshalb läuft es in einem eigenen Task.
static volatile bool s_want = false;
static volatile bool s_starting = false;
static volatile uint32_t s_start_ms = 0;
static volatile bool s_lack_mem = false;
#define VOICE_MIN_FREE_KB 150  // darunter startet die Spracherkennung nicht
static TaskHandle_t s_task = NULL;

static void voice_task(void *arg) {
  while (true) {
    if (s_want && !s_running) {
      s_starting = true;
      s_start_ms = millis();
      mic_set_exclusive(true);  // niemand sonst darf jetzt vom Mikrofon lesen
      mic_listen(false);
      // warten, bis der Pegel-Task wirklich nicht mehr liest
      for (int i = 0; i < 20 && !mic_idle(); i++) vTaskDelay(pdMS_TO_TICKS(50));

      unsigned free_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
      printf("Sprache: Start … frei: %u KB RAM, %u KB PSRAM\r\n", free_kb,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

      if (web_running()) {  // WLAN belegt viel internen Speicher
        printf("Sprache: Weboberfläche wird beendet, sie braucht denselben Speicher\r\n");
        web_stop();
        vTaskDelay(pdMS_TO_TICKS(300));
        free_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
      }

      // Ohne Modell-Partition hängt ESP_SR.begin() – lieber vorher abbrechen
      const esp_partition_t *model = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                              ESP_PARTITION_SUBTYPE_ANY, "model");
      if (free_kb < VOICE_MIN_FREE_KB) {
        // Zu wenig interner Speicher: ESP-SR würde ihn aufbrauchen und
        // danach bekäme die SD-Karte keinen DMA-Speicher mehr.
        printf("Sprache: zu wenig interner Speicher (%u KB frei, %u KB nötig)\r\n", free_kb,
               (unsigned)VOICE_MIN_FREE_KB);
        s_lack_mem = true;
        s_want = false;
      } else if (!model) {
        printf("Sprache: Partition \"model\" fehlt – Schema \"ESP SR 16M\" wählen\r\n");
        s_want = false;
      } else if (!mic_begin()) {
        printf("Sprache: Mikrofon fehlt\r\n");
        s_want = false;
      } else {
        printf("Sprache: Modelle werden geladen …\r\n");
        ESP_SR.onEvent(sr_event);
        if (ESP_SR.begin(mic_i2s(), VOICE_CMDS, sizeof(VOICE_CMDS) / sizeof(sr_cmd_t), SR_CHANNELS_STEREO,
                         SR_MODE_WAKEWORD)) {
          s_running = true;
          s_lack_mem = false;
          lower_sr_priority();
          printf("Sprache: bereit – Weckwort \"Hi ESP\" (noch %u KB RAM frei)\r\n",
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
        } else {
          printf("Sprache: Start fehlgeschlagen (Partition \"ESP SR 16M\" gewählt?)\r\n");
          s_want = false;
        }
      }
      if (!s_running) mic_set_exclusive(false);
      s_starting = false;
    } else if (!s_want && s_running) {
      ESP_SR.end();
      s_running = false;
      mic_set_exclusive(false);
      printf("Sprache: beendet\r\n");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

bool voice_begin() {
  if (s_running || s_starting) return true;
  s_want = true;
  if (!s_task) xTaskCreatePinnedToCore(voice_task, "Voice", 6144, NULL, 5, &s_task, 0);
  return true;  // der Task meldet über voice_running(), ob es geklappt hat
}

bool voice_starting() {
  return s_starting;
}

// hängt der Start? (dann stimmt etwas mit Partition oder Speicher nicht)
bool voice_lack_memory() {
  return s_lack_mem;
}

bool voice_start_stuck() {
  return s_starting && millis() - s_start_ms > 12000;
}

bool voice_available() {
  return true;
}

void voice_stop() {
  s_want = false;  // der Task beendet die Erkennung
}

bool voice_running() {
  return s_running;
}

bool voice_awake() {
  return s_awake;
}

const char *voice_last() {
  return s_last;
}

unsigned long voice_last_ms() {
  return s_last_ms;
}

void voice_loop() {
  if (!s_running || s_pending < 0) return;
  int cmd = s_pending;
  s_pending = -1;
  s_awake = false;
  switch (cmd) {
    case CMD_TARE:
      scale_tare();
      sound_play(SND_TARA);
      break;
    case CMD_SAVE:
      sound_play(log_add(scale_net(), T("Sprachbefehl")) ? SND_SAVE : SND_WARN);
      break;
    case CMD_NEXT: {
      // den fortführenden Knopf der aktuellen Seite drücken
      static const char *const NEXT_LABELS[] = { "Weiter", "Fertig", "Start", "Tippen", "Los",
                                                 "Nächste Runde", "Übernehmen", "Mixen" };
      if (!ui_click_button(NEXT_LABELS, sizeof(NEXT_LABELS) / sizeof(NEXT_LABELS[0]))) sound_play(SND_WARN);
      break;
    }
    case CMD_READ:
      sound_speak_weight(scale_net(), g_set.unit);
      break;
    case CMD_BACK:
      ui_go_home();
      break;
    case CMD_STOP:
      sound_parking_reset();
      ui_go_home();
      break;
  }
}

// neue Funktionen (eigenes Modell, Aufnahmen) gibt es nur ohne ESP-SR
void voice_model_changed() {}
bool voice_model_ok() { return true; }
const char *voice_model_error() { return ""; }
const char *voice_model_info() { return "ESP-SR"; }
const char *voice_model_path() { return ""; }
int voice_live(float *p) { if (p) *p = 0; return -1; }
float voice_level() { return 0; }
int voice_words() { return 0; }
const char *voice_word_id(int i) { return ""; }
const char *voice_word_text(int i) { return ""; }
int voice_word_count(int i) { return 0; }
bool voice_rec_start(int word, bool series) { return false; }
void voice_rec_session(bool on) {}
void voice_hold(bool on) {}
void voice_rec_stop() {}
bool voice_rec_series() { return false; }
bool voice_wake() { return false; }
int voice_rec_state() { return -1; }
float voice_rec_progress() { return 0; }
int voice_rec_ms(int word) { return 0; }
void voice_sim_feed(const int16_t *pcm, int n) {}

#else  // eigene Schlüsselwort-Erkennung (kws.*)
#include "kws.h"
#include "ui.h"
#include "scale.h"
#include "sound.h"
#include "settings.h"
#include "storage.h"
#include "data.h"
#include "hal.h"
#include "i18n.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include "mic.h"
extern I2SClass &mic_i2s();
static void ensure_task();
#endif

#define AWAKE_MS 6000      // so lange nach "Hallo Waage" auf einen Befehl warten
#define DECIDE_MS 450      // nach dem ersten Treffer noch so lange sammeln, dann den besten nehmen
#define DECIDE_ZIEL_MS 1000  // bei Zielmengen länger: "zweihundert…" kommt vor "…fünfzig"
#define AFTER_WAKE_MS 500  // nach dem Weckwort kurz nichts auswerten (Rest von "Waage")
#define AFTER_CMD_MS 1200  // nach einem Befehl dasselbe Wort nicht nochmal erkennen
#define REC_WAIT_MS 350    // nach dem Piepton warten (Ton nicht mitaufnehmen)
#define AFTER_SOUND_MS 250 // nach eigener Ansage/Ton so lange nicht auswerten (Nachhall)
#define REC_DIR DIR_SPEECH "/Aufnahmen"

// ---- Wörter: feste Liste (wie Sprachmodell/kws_common.py) ----
typedef struct {
  const char *id, *text;
  int ms;
} word_t;
// Längen großzügig: nach dem Piepton braucht man eine halbe Sekunde, bis
// man spricht, und ein abgeschnittenes "Hallo Waa…" lehrt das Netz das
// Falsche. Die Stille vorn und hinten schneidet das Training weg.
static const word_t WORDS[] = {
  { "hallo_waage", "Hallo Waage", 3000 },
  { "tara", "Tara", 2500 },
  { "wie_viel", "Wie viel?", 2500 },
  { "speichern", "Speichern", 2500 },
  { "weiter", "Weiter", 2500 },
  { "zurueck", "Zurück", 2500 },
  { "stopp", "Stopp", 2500 },
  { "ziel_50", "50 Gramm", 3000 },
  { "ziel_100", "100 Gramm", 3000 },
  { "ziel_150", "150 Gramm", 3000 },
  { "ziel_200", "200 Gramm", 3000 },
  { "ziel_250", "250 Gramm", 3000 },
  { "ziel_300", "300 Gramm", 3000 },
  { "ziel_500", "500 Gramm", 3000 },
  { "ziel_1000", "1000 Gramm", 3000 },
  { "hilfe", "Hilfe", 2500 },
  { "nochmal", "Nochmal", 2500 },
  { "lauter", "Lauter", 2500 },
  { "leiser", "Leiser", 2500 },
  { "akku", "Akku", 2500 },
  { "uhrzeit", "Uhrzeit", 2500 },
  { "_unbekannt", "ein anderes Wort", 2500 },
  { "_hintergrund", "Küchengeräusche", 10000 },
};
#define SERIES_MS 30000  // Serienaufnahme: dasselbe Wort mehrmals, das Training zerschneidet sie
#define NWORDS_FIX ((int)(sizeof(WORDS) / sizeof(WORDS[0])))
#define WORDS_MAX 40
static char s_extra_id[WORDS_MAX][24], s_extra_text[WORDS_MAX][24];
static int s_nextra = 0;
static int s_counts[WORDS_MAX + NWORDS_FIX];

// ---- Zustand ----
static volatile bool s_want = false;      // Zuhören eingeschaltet
static volatile bool s_hold = false;      // Online-Update läuft: Mikrofon ruht
static bool s_snd_busy = false;           // Waage spricht gerade (Erkennung ruht)
static volatile int s_session = 0;        // offene Aufnahmeseiten (Zähler: neue Seite entsteht vor dem Löschen der alten)
static volatile bool s_running = false;   // Modell geladen, Mikrofon läuft
static volatile bool s_starting = false;
static volatile uint32_t s_start_ms = 0;
static char s_err[64] = "";
static bool s_model_tried = false;
static int s_model_lang = -1;
static uint32_t s_clock = 0;              // Zeit aus der Zahl der Abtastwerte (ms)
static uint32_t s_samples = 0;
static volatile bool s_awake = false;
static uint32_t s_awake_until = 0, s_mute_until = 0, s_dec_end = 0;
static bool s_dec_active = false;
static float s_peak[KWS_MAX_CLASSES];
static uint32_t s_peak_t[KWS_MAX_CLASSES];  // wann die Spitze war
static bool s_dec_ziel = false;             // erster Treffer war eine Zielmenge
static int s_wake_idx = -1;
static volatile int s_pending = -1;       // erkannte Klasse, im UI-Takt ausführen
static volatile bool s_reset_req = true;  // Verlauf leeren (beim Einschalten), passiert im Audio-Takt
static uint32_t s_listen_start = 0;       // erst nach 2 s auswerten (gleitender Mittelwert muss einschwingen)
#define WARMUP_MS 2000
static volatile bool s_ev_wake = false;
static volatile bool s_ev_notund = false; // wach, es wurde etwas gesagt, aber kein Befehl erkannt
static bool s_heard = false;               // seit dem Wecken hat jemand gesprochen
static uint32_t s_awake_since = 0;
static int s_unk_idx = -1;
static char s_last[24] = "";
static volatile unsigned long s_last_ms = 0;
static volatile float s_level = 0;
static volatile int s_live = -1;
static volatile float s_live_p = 0;

// Aufnahme
static volatile int s_rec_state = 0;
static int s_rec_word = -1;
static int16_t *s_rec_buf = NULL;         // 22 Werte Platz für den WAV-Kopf, dann die Aufnahme
static bool s_rec_series = false;         // Serienaufnahme (30 s, Datei s001.wav …)
static int s_rec_len = 0, s_rec_pos = 0, s_rec_wait = 0;
static uint32_t s_rec_done_ms = 0;

const char *voice_model_path() {
  return g_set.lang == LANG_EN ? DIR_SPEECH "/kws_en.bin" : DIR_SPEECH "/kws_de.bin";
}

static void *big_alloc(size_t n) {
#ifdef ARDUINO
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
#else
  return malloc(n);
#endif
}

// ---- Wortliste ----
static void rebuild_extra() {
  s_nextra = 0;
  for (int k = 0; k < kws_num_classes() && s_nextra < WORDS_MAX; k++) {
    const char *c = kws_class(k);
    if (strcmp(c, "_stille") == 0) continue;
    bool known = false;
    for (int i = 0; i < NWORDS_FIX; i++)
      if (strcmp(WORDS[i].id, c) == 0) known = true;
    if (known) continue;
    snprintf(s_extra_id[s_nextra], sizeof(s_extra_id[0]), "%s", c);
    if (strncmp(c, "ziel_", 5) == 0) snprintf(s_extra_text[s_nextra], sizeof(s_extra_text[0]), "%s Gramm", c + 5);
    else snprintf(s_extra_text[s_nextra], sizeof(s_extra_text[0]), "%s", c);
    s_nextra++;
  }
  for (int i = 0; i < WORDS_MAX + NWORDS_FIX; i++) s_counts[i] = -1;
}

int voice_words() { return NWORDS_FIX + s_nextra; }
const char *voice_word_id(int i) {
  if (i >= 0 && i < NWORDS_FIX) return WORDS[i].id;
  i -= NWORDS_FIX;
  return (i >= 0 && i < s_nextra) ? s_extra_id[i] : "";
}
const char *voice_word_text(int i) {
  if (i >= 0 && i < NWORDS_FIX) return WORDS[i].text;
  i -= NWORDS_FIX;
  return (i >= 0 && i < s_nextra) ? s_extra_text[i] : "";
}
int voice_rec_ms(int i) { return (i >= 0 && i < NWORDS_FIX) ? WORDS[i].ms : 1600; }

int voice_word_count(int i) {
  if (i < 0 || i >= voice_words()) return 0;
  if (s_counts[i] >= 0) return s_counts[i];
  // Namensliste nur kurz vom Heap (statisch wären es 10 KB im knappen internen RAM)
  char(*names)[STORAGE_NAME_LEN] = (char(*)[STORAGE_NAME_LEN])big_alloc(300 * STORAGE_NAME_LEN);
  if (!names) return 0;
  char dir[80];
  snprintf(dir, sizeof(dir), REC_DIR "/%s", voice_word_id(i));
  int n = storage_list(dir, names, 300), c = 0;
  for (int k = 0; k < n; k++)
    if (strstr(names[k], ".wav") || strstr(names[k], ".WAV")) c++;
  free(names);
  s_counts[i] = c;
  return c;
}

// ---- Modell ----
static bool load_model() {
  s_model_tried = true;
  s_model_lang = g_set.lang;
  bool ok = kws_load_file(voice_model_path());
  snprintf(s_err, sizeof(s_err), "%s", ok ? "" : kws_error());
  s_wake_idx = ok ? kws_class_index("hallo_waage") : -1;
  s_unk_idx = ok ? kws_class_index("_unbekannt") : -1;
  rebuild_extra();
  printf("Sprache: %s %s\r\n", voice_model_path(), ok ? kws_info() : kws_error());
  return ok;
}

bool voice_model_ok() {
  if (!s_model_tried || s_model_lang != g_set.lang) return storage_size(voice_model_path()) > 0;
  return kws_ready();
}
const char *voice_model_error() {
  if (!s_model_tried && storage_size(voice_model_path()) <= 0) return "kein Modell auf der SD-Karte";
  return s_err;
}
const char *voice_model_info() { return kws_ready() ? kws_info() : ""; }
void voice_model_changed() {
  s_model_tried = false;  // Task (bzw. voice_begin im Simulator) lädt neu
#ifndef ARDUINO
  if (s_want) voice_begin();
#endif
}

// ---- Erkennung: Weckwort, dann Befehl ----
static bool is_cmd(int k) {
  const char *c = kws_class(k);
  return c[0] != '_' && k != s_wake_idx;
}

static void decide() {
  const float *p = kws_probs();
  uint32_t now = s_clock;
  static uint32_t last_log = 0;  // zur Fehlersuche: wie lange eine Auswertung dauert
  if (now - last_log > 30000) {
    last_log = now;
    printf("Sprache: Auswertung %u ms\r\n", (unsigned)(kws_infer_us() / 1000));
  }
  // bester Befehl (für die Mikrofonseite)
  float bp;
  int b = kws_best(&bp);
  s_live = b;
  s_live_p = bp;
  if (s_session || sound_busy() || now < s_mute_until || now - s_listen_start < WARMUP_MS) {
    s_dec_active = false;
    return;
  }
  if (s_wake_idx >= 0 && p[s_wake_idx] >= kws_wake_threshold() && !s_dec_active) {
    if (!s_awake) {
      s_ev_wake = true;
      s_heard = false;
    }
    s_awake_since = now;
    s_awake = true;
    s_awake_until = now + AWAKE_MS;
    s_mute_until = now + AFTER_WAKE_MS;
    return;
  }
  if (!s_awake) return;
  if (!s_dec_active && now > s_awake_until) {
    s_awake = false;
    if (s_heard) s_ev_notund = true;  // etwas gesagt, aber nichts verstanden
    s_heard = false;
    return;
  }
  // Spricht jemand? (irgendein Wort oder "anderes Wort" wahrscheinlich)
  // (das Weckwort selbst zählt nicht, das steht noch kurz im Fenster)
  float cp = 0;
  for (int k = 0; k < kws_num_classes(); k++)
    if (is_cmd(k) && p[k] > cp) cp = p[k];
  bool wake_gone = now - s_awake_since > 1500;  // Rest von "Waage" ist aus dem Fenster
  if (cp >= 0.25f || (wake_gone && s_unk_idx >= 0 && p[s_unk_idx] >= 0.6f)) s_heard = true;
  if (!s_dec_active) {
    bool hit = false;
    for (int k = 0; k < kws_num_classes(); k++)
      if (is_cmd(k) && p[k] >= kws_threshold()) hit = true;
    if (!hit) return;
    s_dec_active = true;
    memset(s_peak, 0, sizeof(s_peak));
    memset(s_peak_t, 0, sizeof(s_peak_t));
    int first = -1;
    float fp = 0;
    for (int k = 0; k < kws_num_classes(); k++)
      if (is_cmd(k) && p[k] > fp) {
        fp = p[k];
        first = k;
      }
    s_dec_ziel = first >= 0 && strncmp(kws_class(first), "ziel_", 5) == 0;
    s_dec_end = now + (s_dec_ziel ? DECIDE_ZIEL_MS : DECIDE_MS);
  }
  for (int k = 0; k < kws_num_classes(); k++)
    if (is_cmd(k) && p[k] > s_peak[k]) {
      s_peak[k] = p[k];
      s_peak_t[k] = now;
    }
  if (now < s_dec_end) return;
  int best = -1;
  float pk = 0;
  if (s_dec_ziel) {
    // Zielmengen: die zuletzt über der Schwelle war, ist die vollständige Zahl
    // ("zweihundert" erreicht seine Spitze vor "zweihundertfünfzig")
    uint32_t bt = 0;
    for (int k = 0; k < kws_num_classes(); k++)
      if (is_cmd(k) && strncmp(kws_class(k), "ziel_", 5) == 0 && s_peak[k] >= kws_threshold() &&
          (s_peak_t[k] > bt || (s_peak_t[k] == bt && s_peak[k] > pk))) {
        bt = s_peak_t[k];
        pk = s_peak[k];
        best = k;
      }
  }
  if (best < 0)  // sonst: höchste Spitze im Sammelfenster
    for (int k = 0; k < kws_num_classes(); k++)
      if (is_cmd(k) && s_peak[k] > pk) {
        pk = s_peak[k];
        best = k;
      }
  s_dec_active = false;
  if (best < 0) return;
  s_pending = best;
  snprintf(s_last, sizeof(s_last), "%s", kws_class(best));
  s_last_ms = hal_millis();
  s_awake_until = now + AWAKE_MS;  // gleich noch ein Befehl möglich ("weiter", "weiter")
  s_mute_until = now + AFTER_CMD_MS;
}

// ---- Aufnahme speichern (WAV, 16 kHz, Mono) ----
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

static void rec_save() {
  const char *id = voice_word_id(s_rec_word);
  char dir[80], path[100];
  storage_mkdir(REC_DIR);
  snprintf(dir, sizeof(dir), REC_DIR "/%s", id);
  storage_mkdir(dir);
  int n = 1;
  do {
    snprintf(path, sizeof(path), "%s/%c%03d.wav", dir, s_rec_series ? 's' : 'a', n++);
  } while (storage_exists(path) && n < 1000);
  uint8_t *h = (uint8_t *)s_rec_buf;
  uint32_t bytes = s_rec_len * 2;
  memcpy(h, "RIFF", 4);
  put32(h + 4, 36 + bytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(h + 16, 16);
  put16(h + 20, 1);
  put16(h + 22, 1);
  put32(h + 24, KWS_RATE);
  put32(h + 28, KWS_RATE * 2);
  put16(h + 32, 2);
  put16(h + 34, 16);
  memcpy(h + 36, "data", 4);
  put32(h + 40, bytes);
  bool ok = storage_write_bin(path, s_rec_buf, 44 + bytes);
  free(s_rec_buf);
  s_rec_buf = NULL;
  if (ok && s_counts[s_rec_word] >= 0) s_counts[s_rec_word]++;
  printf("Sprache: Aufnahme %s %s\r\n", path, ok ? "gespeichert" : "fehlgeschlagen");
  s_rec_state = ok ? 4 : -1;
  s_rec_done_ms = hal_millis();
}

// Ein Block Audio (16 kHz Mono) aus dem Mikrofon bzw. dem Simulator
static void process(const int16_t *pcm, int n) {
  // Pegel für Anzeigen (Spitze des Blocks, langsam fallend)
  int mx = 0;
  for (int i = 0; i < n; i++) {
    int a = pcm[i] < 0 ? -pcm[i] : pcm[i];
    if (a > mx) mx = a;
  }
  float lv = mx / 32768.0f;
  s_level = lv > s_level ? lv : s_level * 0.92f;
  s_samples += n;
  s_clock = s_samples / (KWS_RATE / 1000);
  // Aufnahme
  if (s_rec_state == 1) {
    s_rec_wait -= n;
    if (s_rec_wait <= 0) s_rec_state = 2;
  } else if (s_rec_state == 2 && s_rec_buf) {
    int k = s_rec_len - s_rec_pos;
    if (k > n) k = n;
    memcpy(s_rec_buf + 22 + s_rec_pos, pcm, k * 2);
    s_rec_pos += k;
    if (s_rec_pos >= s_rec_len) s_rec_state = 3;  // speichern übernimmt der Task bzw. gleich unten
  }
  // Erkennung. Während die Waage selbst spricht oder piept, wird gar nicht
  // gerechnet: spart Rechenzeit (die Ansage läuft auf demselben Kern) und
  // die Waage hört sich nicht selbst. Danach kurz warten (Nachhall).
  if (sound_busy()) {
    s_snd_busy = true;
    return;
  }
  if (s_snd_busy) {
    s_snd_busy = false;
    if (s_mute_until < s_clock + AFTER_SOUND_MS) s_mute_until = s_clock + AFTER_SOUND_MS;
  }
  if (s_want && kws_ready()) {
    if (s_reset_req) {
      s_reset_req = false;
      kws_reset();
      s_listen_start = s_clock;
      s_awake = false;
    }
    if (kws_feed(pcm, n)) decide();
  }
}

bool voice_rec_start(int word, bool series) {
  if (word < 0 || word >= voice_words() || !storage_ok()) return false;
  if (s_rec_state == 1 || s_rec_state == 2 || s_rec_state == 3) return false;
  int ms = series ? SERIES_MS : voice_rec_ms(word);
  s_rec_series = series;
  s_rec_len = KWS_RATE / 1000 * ms;
  s_rec_buf = (int16_t *)big_alloc((22 + s_rec_len) * sizeof(int16_t));
  if (!s_rec_buf) return false;
  s_rec_word = word;
  s_rec_pos = 0;
  s_rec_wait = KWS_RATE / 1000 * REC_WAIT_MS;
  s_rec_state = 1;
#ifdef ARDUINO
  ensure_task();
#endif
  return true;
}

// Serie vorzeitig beenden: das bisher Aufgenommene speichern
void voice_rec_stop() {
  if (s_rec_state == 1) {  // noch nicht angefangen: verwerfen
    s_rec_state = 0;
    free(s_rec_buf);
    s_rec_buf = NULL;
    return;
  }
  if (s_rec_state != 2) return;
  if (s_rec_pos < KWS_RATE) {  // weniger als 1 s: verwerfen
    s_rec_state = 0;
    free(s_rec_buf);
    s_rec_buf = NULL;
    return;
  }
  s_rec_len = s_rec_pos;
  s_rec_state = 3;
}

bool voice_rec_series() { return s_rec_series; }

// Wecken ohne Weckwort (Doppeltipp auf das Display)
bool voice_wake() {
  if (!s_running || s_session) return false;
  s_awake = true;
  s_heard = false;
  s_dec_active = false;
  s_awake_until = s_clock + AWAKE_MS;
  s_awake_since = s_clock - 2000;  // kein Weckwort im Fenster
  s_mute_until = s_clock + 200;
  s_ev_wake = true;
  return true;
}

int voice_rec_state() {
  // "gespeichert" nach 1,2 s wieder auf bereit
  if ((s_rec_state == 4 || s_rec_state == -1) && hal_millis() - s_rec_done_ms > 1200) s_rec_state = 0;
  return s_rec_state;
}
float voice_rec_progress() {
  if (s_rec_state == 2) return s_rec_len ? (float)s_rec_pos / s_rec_len : 0;
  return s_rec_state >= 3 ? 1.0f : 0.0f;
}
float voice_level() { return s_level; }
int voice_live(float *p) {
  if (p) *p = s_live_p;
  return s_live;
}

bool voice_available() { return true; }
bool voice_running() { return s_running; }
bool voice_awake() { return s_awake; }
bool voice_starting() { return s_starting; }
bool voice_start_stuck() { return s_starting && hal_millis() - s_start_ms > 12000; }
bool voice_lack_memory() { return strstr(s_err, "Speicher") != NULL; }
const char *voice_last() { return s_last; }
unsigned long voice_last_ms() { return s_last_ms; }

// ---- Befehle ausführen (UI-Takt) ----
void voice_loop() {
#ifndef ARDUINO
  if (s_rec_state == 3 && s_rec_buf) rec_save();  // Simulator: kein eigener Task
#endif
  if (s_ev_wake) {
    s_ev_wake = false;
    if (!sound_say("ja")) sound_play_tone(SND_TEST);  // "Ja?" (ohne Stimme: kurzer Ton)
  }
  if (s_ev_notund) {
    s_ev_notund = false;
    if (!sound_say("nicht_verst")) sound_play(SND_WARN);  // "Nicht verstanden. Sag Hilfe."
  }
  if (s_awake && s_clock > s_awake_until + 200 && !s_dec_active) {
    s_awake = false;
    s_heard = false;
  }
  int cmd = s_pending;
  if (cmd < 0) return;
  s_pending = -1;
  const char *c = kws_class(cmd);
  printf("Sprache: Befehl %s\r\n", c);
  if (strcmp(c, "tara") == 0) {
    ui_cmd_tara();
    if (!g_set.speak_ev) sound_say("tara");  // Bestätigung auch ohne Sprüche
  } else if (strcmp(c, "wie_viel") == 0) {
    ui_cmd_speak();
  } else if (strcmp(c, "speichern") == 0) {
    bool ok = log_add(scale_net(), T("Sprachbefehl"));
    sound_play(ok ? SND_SAVE : SND_WARN);
    if (ok && !g_set.speak_ev) sound_say("gespeichert");
  } else if (strcmp(c, "weiter") == 0) {
    static const char *const NEXT_LABELS[] = { "Weiter", "Fertig", "Start", "Tippen", "Los",
                                               "Nächste Runde", "Übernehmen", "Mixen" };
    if (!ui_click_button(NEXT_LABELS, sizeof(NEXT_LABELS) / sizeof(NEXT_LABELS[0]))) sound_play(SND_WARN);
  } else if (strcmp(c, "zurueck") == 0) {
    ui_go_home();
  } else if (strcmp(c, "stopp") == 0) {
    if (ui_target_goal() > 0) {
      ui_target_stop();
      if (!sound_say("ziel_beendet")) sound_play(SND_DONE);
    } else {
      sound_parking_reset();
      ui_go_home();
    }
  } else if (strncmp(c, "ziel_", 5) == 0) {
    int g = atoi(c + 5);
    if (g > 0 && !ui_target_start(g)) sound_play(SND_WARN);  // sagt "Ziel … Gramm. Jetzt einfüllen."
  } else if (strcmp(c, "hilfe") == 0) {
    if (!sound_say("hilfe")) sound_play(SND_WARN);
  } else if (strcmp(c, "nochmal") == 0) {
    if (!sound_repeat()) sound_play(SND_WARN);
  } else if (strcmp(c, "lauter") == 0 || strcmp(c, "leiser") == 0) {
    int v = g_set.volume + (c[1] == 'a' ? 20 : -20);  // lauter / leiser
    g_set.volume = v > 100 ? 100 : (v < 20 ? 20 : v);
    settings_save();
    sound_play_tone(SND_TEST);  // so laut ist es jetzt
  } else if (strcmp(c, "akku") == 0) {
    bat_state_t st = hal_battery_state();
    if (!sound_say_battery(hal_battery_percent(), st == BAT_CHARGING || st == BAT_FULL)) sound_play(SND_WARN);
  } else if (strcmp(c, "uhrzeit") == 0) {
    int h, mi;
    if (!hal_time(&h, &mi) || !sound_say_time(h, mi)) sound_play(SND_WARN);
  }
  s_awake = false;  // Befehl ausgeführt: wieder schlafen, nächstes Mal mit "Hallo Waage"
}

#ifdef ARDUINO
// ------------------------------------------------------------
//  Gerät: eigener Task auf Kern 0 liest das Mikrofon
// ------------------------------------------------------------
static TaskHandle_t s_task = NULL;
static bool s_mic_on = false;

static void voice_task(void *arg) {
  static int16_t raw[512];  // 256 Stereo-Paare = 16 ms
  static int16_t mono[256];
  while (true) {
    bool need = (s_want || s_session || s_rec_state == 1 || s_rec_state == 2) && !s_hold;
    if (!need) {
      if (s_mic_on) {
        mic_set_exclusive(false);
        s_mic_on = false;
        s_running = false;
        s_awake = false;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (!s_mic_on) {
      mic_set_exclusive(true);  // Pegel-Task liest nicht mehr
      mic_listen(false);
      for (int i = 0; i < 20 && !mic_idle(); i++) vTaskDelay(pdMS_TO_TICKS(50));
      if (!mic_begin()) {
        snprintf(s_err, sizeof(s_err), "Mikrofon fehlt");
        mic_set_exclusive(false);
        s_want = false;
        s_session = 0;
        s_starting = false;
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
      }
      s_mic_on = true;
    }
    if (s_want && (!s_model_tried || s_model_lang != g_set.lang)) {
      s_starting = true;
      s_start_ms = hal_millis();
      load_model();
      s_starting = false;
      if (!kws_ready()) s_want = false;  // ohne Modell nicht zuhören
    }
    s_running = s_want && kws_ready();
    if (s_rec_state == 3 && s_rec_buf) {
      rec_save();
      continue;
    }
    size_t got = mic_i2s().readBytes((char *)raw, sizeof(raw));
    int frames = (int)got / 4;
    if (frames <= 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    // beide Kanäle zusammen: das Mikrofon liegt je nach Verdrahtung links oder rechts
    for (int i = 0; i < frames; i++) {
      int v = (int)raw[2 * i] + raw[2 * i + 1];
      mono[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
    process(mono, frames);
  }
}

static void ensure_task() {
  if (!s_task) xTaskCreatePinnedToCore(voice_task, "Voice", 8192, NULL, 3, &s_task, 0);
}

bool voice_begin() {
  s_want = true;
  s_reset_req = true;
  if (!s_model_tried) s_starting = true;
  ensure_task();
  return true;
}

void voice_stop() {
  s_want = false;
  s_awake = false;
}

void voice_rec_session(bool on) {
  s_session += on ? 1 : (s_session > 0 ? -1 : 0);
  if (on) ensure_task();
}

void voice_hold(bool on) {
  if (s_hold == on) return;
  s_hold = on;
  if (!on) s_reset_req = true;  // danach mit leerem Verlauf weiter
}

void voice_sim_feed(const int16_t *pcm, int n) {}

#else
// ------------------------------------------------------------
//  Simulator: Modell sofort laden, Audio kommt über voice_sim_feed
// ------------------------------------------------------------
bool voice_begin() {
  s_want = true;
  s_reset_req = true;
  if (!s_model_tried || s_model_lang != g_set.lang) load_model();
  s_running = kws_ready();
  if (!kws_ready()) s_want = false;
  return s_running;
}
void voice_stop() {
  s_want = false;
  s_running = false;
  s_awake = false;
}
void voice_rec_session(bool on) { s_session += on ? 1 : (s_session > 0 ? -1 : 0); }
void voice_hold(bool on) { s_hold = on; }
void voice_sim_feed(const int16_t *pcm, int n) {
  while (n > 0) {
    int k = n > 256 ? 256 : n;
    process(pcm, k);
    if (s_rec_state == 3 && s_rec_buf) rec_save();
    pcm += k;
    n -= k;
  }
}
#endif
#endif
