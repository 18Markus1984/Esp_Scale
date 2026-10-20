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
static char s_when[24] = "";  // Zeitpunkt des Absturzes ("2026-10-14 08:31:05"), "" = unbekannt

const char *crash_text() { return s_text; }
const char *crash_reason() { return s_reason; }
const char *crash_where() { return s_where; }
const char *crash_when() { return s_when; }
int crash_pct() { return s_pct; }
bool crash_last_abnormal() { return s_abnormal; }

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_core_dump.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "storage.h"
#include "hal.h"
#include "update_online.h"

#define CRASH_FILE "/Waage/Absturz.txt"
#define PHASE_MAGIC 0x57414147u  // "WAAG"
#define HB_MAGIC 0x48425754u     // "HBWT"
#define HB_TASKS 6               // so viele Tasks mit dem knappsten Stapel merken
#define HB_TIME_MS 1000          // Zeit, Laufzeit, Speicher: jede Sekunde
#define HB_TASK_MS 10000         // Stapel aller Tasks: alle 10 s

// Überlebt einen Neustart ohne Stromausfall (Absturz, Watchdog, esp_restart)
static RTC_NOINIT_ATTR struct {
  uint32_t magic;
  char what[40];
  int pct;
} s_phase;

// "Lebenszeichen": was kurz vor einem Absturz los war. Wird laufend
// überschrieben und nach dem Neustart in den Bericht übernommen.
typedef struct {
  char name[12];
  uint32_t free;  // Bytes, die vom Stapel nie benutzt wurden
} hb_task_t;
typedef struct {
  uint32_t magic;
  uint16_t y;
  uint8_t mo, d, h, mi, s, time_ok;
  uint32_t up_s;                       // Laufzeit seit dem Einschalten
  uint32_t heap_free, heap_min, heap_block, psram_free;
  uint8_t ntask;
  hb_task_t task[HB_TASKS];
} hb_t;
static RTC_NOINIT_ATTR hb_t s_hb;

// Abstürze kurz nacheinander (überlebt den Neustart, nicht Strom aus)
#define QUICK_MAGIC 0x51434b31u
#define QUICK_UP_S 120  // so kurz nach dem Start gilt ein Absturz als "schnell"
static RTC_NOINIT_ATTR struct {
  uint32_t magic;
  uint32_t n;
} s_quick;
static bool s_safe = false;
bool crash_safe_mode() { return s_safe; }

// Kopie des Lebenszeichens vom letzten Lauf (nur nach Absturz gültig)
static bool s_last_ok = false;
static hb_t s_last;

// Berichtzeile, wird geschrieben, sobald SD-Karte und Uhrzeit da sind
#define LINE_LEN 700
static char *s_line = NULL;  // im PSRAM, nur nach einem Absturz
static bool s_pending = false;
static uint32_t s_boot_ms = 0;

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

// Ausnahme-Ursache (Xtensa EXCCAUSE) auf Deutsch
static const char *cause_text(uint32_t c) {
  switch (c) {
    case 0: return "ungültiger Befehl";
    case 2: return "Befehl nicht lesbar";
    case 3: return "Speicherfehler beim Laden/Speichern";
    case 6: return "Division durch null";
    case 9: return "falsch ausgerichteter Zugriff";
    case 20: return "Sprung an ungültige Adresse";
    case 28: return "Lesen von ungültiger Adresse";
    case 29: return "Schreiben an ungültige Adresse";
    default: return c >= 64 ? "Sonderfall (Debug-Ausnahme, Stapel-Wächter oder Watchdog)" : "Ausnahme";
  }
}

static int fmt_dur(char *b, int len, uint32_t s) {
  if (s >= 3600) return snprintf(b, len, "%lu h %02lu min", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
  if (s >= 60) return snprintf(b, len, "%lu min %02lu s", (unsigned long)(s / 60), (unsigned long)(s % 60));
  return snprintf(b, len, "%lu s", (unsigned long)s);
}

// ------------------------------------------------------------
//  Lebenszeichen (aus dem UI-Takt)
// ------------------------------------------------------------
static void hb_tasks() {
  UBaseType_t n = uxTaskGetNumberOfTasks() + 4;
  TaskStatus_t *st = (TaskStatus_t *)heap_caps_malloc(n * sizeof(TaskStatus_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!st) return;
  n = uxTaskGetSystemState(st, n, NULL);
  // die HB_TASKS Tasks mit dem wenigsten freien Stapel (ESP-IDF zählt in Bytes)
  hb_task_t best[HB_TASKS];
  int k = 0;
  for (; k < HB_TASKS && k < (int)n; k++) {
    int m = k;
    for (UBaseType_t i = k + 1; i < n; i++)
      if (st[i].usStackHighWaterMark < st[m].usStackHighWaterMark) m = i;
    TaskStatus_t t = st[k];
    st[k] = st[m];
    st[m] = t;
    snprintf(best[k].name, sizeof(best[k].name), "%s", st[k].pcTaskName);
    best[k].free = st[k].usStackHighWaterMark;
  }
  free(st);
  memcpy(s_hb.task, best, sizeof(hb_task_t) * k);
  s_hb.ntask = k;
}

void crash_tick() {
  static uint32_t last = 0, last_tasks = 0;
  uint32_t now = millis();
  if (s_pending) {  // Bericht vom letzten Absturz noch nicht geschrieben
    int y, mo, d, h, mi, sec;
    bool t = hal_now(&y, &mo, &d, &h, &mi, &sec);
    if (storage_ok() && (t || s_when[0] || now - s_boot_ms > 15000)) {
      if (!s_when[0] && t) snprintf(s_when, sizeof(s_when), "%04d-%02d-%02d %02d:%02d (Neustart)", y, mo, d, h, mi);
      char *line = (char *)heap_caps_malloc(LINE_LEN + 40, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!line) return;
      // alte Datei ohne Überschrift (bis 1.1.18): neu anfangen, die alte als Absturz_alt.txt behalten
      char head[12] = "";
      if (storage_read_bin(CRASH_FILE, head, 9) > 0 && strncmp(head, "Zeitpunkt", 9) != 0) {
        long n = storage_size(CRASH_FILE);
        char *old = n > 0 && n < 65536 ? (char *)heap_caps_malloc(n + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
        if (old && storage_read(CRASH_FILE, old, n + 1) > 0) storage_write("/Waage/Absturz_alt.txt", old);
        free(old);
        storage_remove(CRASH_FILE);
      }
      bool fresh = storage_size(CRASH_FILE) <= 0;
      if (fresh)
        storage_append(CRASH_FILE, "Zeitpunkt;Version;Laufzeit;Neustartgrund;Tätigkeit;Task;Fehler;Meldung;"
                                   "Rücksprungadressen;Speicher intern frei/kleinster Stand/größter Block;"
                                   "Stapel frei (knappste Tasks)");
      snprintf(line, LINE_LEN + 40, "%s;%s", s_when[0] ? s_when : "unbekannt", s_line);
      storage_append(CRASH_FILE, line);
      free(line);
      s_pending = false;
      free(s_line);
      s_line = NULL;
    }
  }
  if (now - last < HB_TIME_MS) return;
  last = now;
  int y, mo, d, h, mi, sec;
  s_hb.time_ok = hal_now(&y, &mo, &d, &h, &mi, &sec) ? 1 : 0;
  if (s_hb.time_ok) {
    s_hb.y = y;
    s_hb.mo = mo;
    s_hb.d = d;
    s_hb.h = h;
    s_hb.mi = mi;
    s_hb.s = sec;
  }
  s_hb.up_s = now / 1000;
  s_hb.heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  s_hb.heap_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  s_hb.heap_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  s_hb.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  if (now - last_tasks >= HB_TASK_MS || s_hb.magic != HB_MAGIC) {
    last_tasks = now;
    hb_tasks();
  }
  s_hb.magic = HB_MAGIC;
}

// ------------------------------------------------------------
//  Beim Start: Bericht vom letzten Lauf
// ------------------------------------------------------------
void crash_begin() {
  s_boot_ms = millis();
  esp_reset_reason_t r = esp_reset_reason();
  s_abnormal = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
               r == ESP_RST_BROWNOUT;
  bool phase = s_phase.magic == PHASE_MAGIC && r != ESP_RST_POWERON;
  s_phase.what[sizeof(s_phase.what) - 1] = 0;
  s_last_ok = s_hb.magic == HB_MAGIC && r != ESP_RST_POWERON;
  if (s_last_ok) memcpy(&s_last, &s_hb, sizeof(s_last));
  s_hb.magic = 0;  // ab jetzt neu
  s_hb.ntask = 0;

  // Core-Dump (nur nach einem Absturz vorhanden): Task, Ursache, Adressen, Meldung
  char task[20] = "", fault[96] = "", why[200] = "", bt[140] = "";
  if (esp_core_dump_image_check() == ESP_OK) {
    esp_core_dump_summary_t *sm = (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
    if (sm && esp_core_dump_get_summary(sm) == ESP_OK) {
      snprintf(task, sizeof(task), "%.16s", sm->exc_task[0] ? sm->exc_task : "?");
      uint32_t c = sm->ex_info.exc_cause;
      if (c == 28 || c == 29)
        snprintf(fault, sizeof(fault), "%s (Ursache %lu, Adresse 0x%08lx, PC 0x%08lx)", cause_text(c), (unsigned long)c,
                 (unsigned long)sm->ex_info.exc_vaddr, (unsigned long)sm->exc_pc);
      else
        snprintf(fault, sizeof(fault), "%s (Ursache %lu, PC 0x%08lx)", cause_text(c), (unsigned long)c,
                 (unsigned long)sm->exc_pc);
      int o = 0;
      for (uint32_t i = 0; i < sm->exc_bt_info.depth && i < 12 && o < (int)sizeof(bt) - 12; i++)
        o += snprintf(bt + o, sizeof(bt) - o, "%s0x%08lx", i ? " " : "", (unsigned long)sm->exc_bt_info.bt[i]);
      if (sm->exc_bt_info.corrupted && o < (int)sizeof(bt) - 12) snprintf(bt + o, sizeof(bt) - o, " (unvollständig)");
    }
    free(sm);
    if (esp_core_dump_get_panic_reason(why, sizeof(why)) != ESP_OK) why[0] = 0;
    for (char *p = why; *p; p++)
      if (*p == ';' || *p == '\n' || *p == '\r') *p = ' ';  // Spalten nicht zerreißen
    if (strncmp(why, "assert", 6) == 0 || strncmp(why, "abort", 5) == 0) snprintf(fault, sizeof(fault), "Abbruch (assert/abort)");
    esp_core_dump_image_erase();  // nur einmal melden
  }

  // Sicherer Start (siehe crash.h)
  if (s_quick.magic != QUICK_MAGIC || r == ESP_RST_POWERON) s_quick.n = 0;
  s_quick.magic = QUICK_MAGIC;
  bool quick = s_abnormal && r != ESP_RST_BROWNOUT && s_last_ok && s_last.up_s < QUICK_UP_S;
  s_quick.n = quick ? s_quick.n + 1 : 0;
  bool voice_task = !strcmp(task, "Voice") || !strcmp(task, "MicRd");
  s_safe = (quick && voice_task) || s_quick.n >= 3;
  if (s_safe) printf("Start: sicherer Start nach %lu schnellen Abstürzen (Task %s), Sprachsteuerung bleibt aus\r\n",
                     (unsigned long)s_quick.n, task[0] ? task : "?");

  if (!s_abnormal && !phase) {
    s_phase.magic = 0;
    printf("Start: %s\r\n", reason_text(r));
    return;
  }
  s_reason = reason_text(r);
  int o = snprintf(s_text, sizeof(s_text), "%s", s_reason);
  if (s_safe) o += snprintf(s_text + o, sizeof(s_text) - o, " – Sprachsteuerung vorerst aus");
  if (phase) {
    snprintf(s_where, sizeof(s_where), "%s", s_phase.what);
    s_pct = s_phase.pct;
    o += snprintf(s_text + o, sizeof(s_text) - o, " – %s (%d %%)", s_phase.what, s_phase.pct);
  }
  s_phase.magic = 0;

  // Zeitpunkt aus dem letzten Lebenszeichen (höchstens 1 s vor dem Absturz)
  char up[24] = "?", mem[64] = "", stacks[160] = "";
  if (s_last_ok) {
    if (s_last.time_ok)
      snprintf(s_when, sizeof(s_when), "%04u-%02u-%02u %02u:%02u:%02u", s_last.y, s_last.mo, s_last.d, s_last.h,
               s_last.mi, s_last.s);
    fmt_dur(up, sizeof(up), s_last.up_s);
    snprintf(mem, sizeof(mem), "%lu/%lu/%lu KB", (unsigned long)(s_last.heap_free / 1024),
             (unsigned long)(s_last.heap_min / 1024), (unsigned long)(s_last.heap_block / 1024));
    int k = 0;
    for (int i = 0; i < s_last.ntask && i < HB_TASKS; i++) {
      s_last.task[i].name[sizeof(s_last.task[i].name) - 1] = 0;
      k += snprintf(stacks + k, sizeof(stacks) - k, "%s%s %lu", i ? ", " : "", s_last.task[i].name,
                    (unsigned long)s_last.task[i].free);
      if (k >= (int)sizeof(stacks)) break;
    }
  }
  printf("Start: %s, zuletzt um %s nach %s Laufzeit\r\n", s_text, s_when[0] ? s_when : "?", up);
  if (task[0]) printf("Absturz: Task %s, %s, %s, BT %s\r\n", task, fault, why, bt);
  if (stacks[0]) printf("Absturz: Speicher %s, Stapel frei: %s\r\n", mem, stacks);

  // Zeile ohne Zeitpunkt vorbereiten; geschrieben wird in crash_tick()
  if (s_abnormal && !s_line) s_line = (char *)heap_caps_malloc(LINE_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (s_abnormal && s_line) {
    char act[56] = "";
    if (phase) snprintf(act, sizeof(act), "%s (%d %%)", s_where, s_pct);
    snprintf(s_line, LINE_LEN, "%s;%s;%s;%s;%s;%s;%s;%s;%s;%s", fw_version(), up, s_reason, act, task, fault,
             why, bt, mem, stacks);
    s_pending = true;
  }
}

#else
// Simulator: immer normal eingeschaltet (Test mit Umgebungsvariable CRASH)
#include <stdlib.h>
bool crash_safe_mode() { return getenv("SAFEMODE") != NULL; }
void crash_phase(const char *what, int pct) {}
void crash_tick() {}
void crash_begin() {
  const char *c = getenv("CRASH");
  if (c && *c) {
    snprintf(s_text, sizeof(s_text), "%s – Beim Laden des Updates (37 %%)", c);
    s_reason = "Absturz";
    snprintf(s_where, sizeof(s_where), "Beim Laden des Updates");
    snprintf(s_when, sizeof(s_when), "2026-10-14 08:31:05");
    s_pct = 37;
    s_abnormal = true;
  }
}
#endif
