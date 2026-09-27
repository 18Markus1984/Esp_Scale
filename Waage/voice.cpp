// ============================================================
//  Spracherkennung (ESP-SR, nur ESP32-S3)
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

#else  // ohne Spracherkennung

bool voice_available() { return false; }
bool voice_begin() { return false; }
bool voice_starting() { return false; }
bool voice_start_stuck() { return false; }
bool voice_lack_memory() { return false; }
void voice_stop() {}
bool voice_running() { return false; }
bool voice_awake() { return false; }
void voice_loop() {}
const char *voice_last() { return ""; }
unsigned long voice_last_ms() { return 0; }

#endif
