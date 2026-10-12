// ============================================================
//  Absturzbericht (siehe crash.h)
// ============================================================
#include "crash.h"
#include <stdio.h>
#include <string.h>

static char s_text[200] = "";
static bool s_abnormal = false;
static const char *s_reason = "";
static char s_where[40] = "";
static int s_pct = 0;

const char *crash_text() { return s_text; }
const char *crash_reason() { return s_reason; }
const char *crash_where() { return s_where; }
int crash_pct() { return s_pct; }
bool crash_last_abnormal() { return s_abnormal; }

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_core_dump.h>
#include "storage.h"
#include "hal.h"
#include "update_online.h"

#define CRASH_FILE "/Waage/Absturz.txt"
#define PHASE_MAGIC 0x57414147u  // "WAAG"

// Überlebt einen Neustart ohne Stromausfall (Absturz, Watchdog, esp_restart)
static RTC_NOINIT_ATTR struct {
  uint32_t magic;
  char what[40];
  int pct;
} s_phase;

void crash_phase(const char *what, int pct) {
  if (!what) {
    s_phase.magic = 0;
    return;
  }
  snprintf(s_phase.what, sizeof(s_phase.what), "%s", what);
  s_phase.pct = pct;
  s_phase.magic = PHASE_MAGIC;
}

static const char *reason_text(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "eingeschaltet";
    case ESP_RST_SW: return "Neustart durch die Firmware";
    case ESP_RST_PANIC: return "Absturz";
    case ESP_RST_INT_WDT: return "Watchdog (Interrupt)";
    case ESP_RST_TASK_WDT: return "Watchdog (Task hängt)";
    case ESP_RST_WDT: return "Watchdog";
    case ESP_RST_BROWNOUT: return "Spannungseinbruch (Akku/Kabel)";
    case ESP_RST_DEEPSLEEP: return "aus dem Tiefschlaf";
    case ESP_RST_EXT: return "Reset-Pin";
    case ESP_RST_USB: return "USB-Reset";
    default: return "unbekannt";
  }
}

void crash_begin() {
  esp_reset_reason_t r = esp_reset_reason();
  s_abnormal = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
               r == ESP_RST_BROWNOUT;
  bool phase = s_phase.magic == PHASE_MAGIC && r != ESP_RST_POWERON;
  s_phase.what[sizeof(s_phase.what) - 1] = 0;

  // Core-Dump (nur nach einem Absturz vorhanden): Task, Grund, Adressen
  char dump[160] = "";
  if (esp_core_dump_image_check() == ESP_OK) {
    esp_core_dump_summary_t *sm = (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
    if (sm && esp_core_dump_get_summary(sm) == ESP_OK) {
      int o = snprintf(dump, sizeof(dump), "Task %.16s, PC 0x%08lx, Ursache %lu, BT", sm->exc_task,
                       (unsigned long)sm->exc_pc, (unsigned long)sm->ex_info.exc_cause);
      for (uint32_t i = 0; i < sm->exc_bt_info.depth && i < 8 && o < (int)sizeof(dump) - 12; i++)
        o += snprintf(dump + o, sizeof(dump) - o, " 0x%08lx", (unsigned long)sm->exc_bt_info.bt[i]);
    }
    free(sm);
    char why[64] = "";
    if (esp_core_dump_get_panic_reason(why, sizeof(why)) == ESP_OK && dump[0]) {
      int l = strlen(dump);
      snprintf(dump + l, sizeof(dump) - l, " (%s)", why);
    }
    esp_core_dump_image_erase();  // nur einmal melden
  }

  if (!s_abnormal && !phase) {
    s_phase.magic = 0;
    printf("Start: %s\r\n", reason_text(r));
    return;
  }
  s_reason = reason_text(r);
  int o = snprintf(s_text, sizeof(s_text), "%s", s_reason);
  if (phase) {
    snprintf(s_where, sizeof(s_where), "%s", s_phase.what);
    s_pct = s_phase.pct;
    o += snprintf(s_text + o, sizeof(s_text) - o, " – %s (%d %%)", s_phase.what, s_phase.pct);
  }
  s_phase.magic = 0;
  printf("Start: %s\r\n", s_text);
  if (dump[0]) printf("Absturz: %s\r\n", dump);

  // Zeile in /Waage/Absturz.txt: Datum Uhrzeit; Version; Grund; Core-Dump
  if (s_abnormal && storage_ok()) {
    int y, mo, d, h, mi, sec;
    char line[400], when[24] = "?";
    if (hal_now(&y, &mo, &d, &h, &mi, &sec)) snprintf(when, sizeof(when), "%04d-%02d-%02d %02d:%02d", y, mo, d, h, mi);
    snprintf(line, sizeof(line), "%s; %s; %s; %s", when, fw_version(), s_text, dump);
    storage_append(CRASH_FILE, line);
  }
}

#else
// Simulator: immer normal eingeschaltet (Test mit Umgebungsvariable CRASH)
#include <stdlib.h>
void crash_phase(const char *what, int pct) {}
void crash_begin() {
  const char *c = getenv("CRASH");
  if (c && *c) {
    snprintf(s_text, sizeof(s_text), "%s – Beim Laden des Updates (37 %%)", c);
    s_reason = "Absturz";
    snprintf(s_where, sizeof(s_where), "Beim Laden des Updates");
    s_pct = 37;
    s_abnormal = true;
  }
}
#endif
