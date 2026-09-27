#include "storage.h"
#include "config.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "TCA9554PWR.h"

// Pins der TF-Karte (siehe Waveshare-Wiki)
#define SD_PIN_CLK 14
#define SD_PIN_CMD 17
#define SD_PIN_D0  16

#define REMOUNT_EVERY_MS 3000   // nach einem Ausfall alle 3 s neu versuchen
#define CHECK_EVERY_MS 20000    // alle 20 s prüfen, ob die Karte noch antwortet

static bool s_ok = false;
static bool s_ever_ok = false;          // war schon einmal eine Karte da?
static uint32_t s_last_try = 0, s_last_check = 0;
static uint32_t s_errors = 0, s_remounts = 0;
static SemaphoreHandle_t s_mtx = NULL;  // Zugriff aus UI- und Ton-Task

// ------------------------------------------------------------
//  Sperre: SD-Karte immer nur von einem Task gleichzeitig benutzen
// ------------------------------------------------------------
void storage_lock() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
  xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY);
}

void storage_unlock() {
  if (s_mtx) xSemaphoreGiveRecursive(s_mtx);
}

struct Lock {  // sperrt bis zum Ende des Blocks
  Lock() { storage_lock(); }
  ~Lock() { storage_unlock(); }
};

// Eine Datei ließ sich nicht öffnen: fehlt nur die Datei, oder ist die Karte weg?
static void check_card() {
  if (!SD_MMC.exists(DIR_ROOT)) {
    s_errors++;
    s_ok = false;
    printf("SD: Karte antwortet nicht mehr (Fehler %lu)\r\n", (unsigned long)s_errors);
  }
}

static bool mount() {
  if (!SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0)) return false;
  // D3 kurz auf Low und wieder auf High: bringt eine hängende Karte zurück
  Set_EXIO(EXIO_PIN4, Low);
  vTaskDelay(pdMS_TO_TICKS(30));
  Set_EXIO(EXIO_PIN4, High);
  vTaskDelay(pdMS_TO_TICKS(30));
  // 1-Bit-Modus, nicht formatieren, gedrosselter Takt (Standard wären 40 MHz,
  // das ist mit Lautsprecher und WLAN zu störanfällig)
  if (!SD_MMC.begin("/sdcard", true, false, SD_FREQ_KHZ)) return false;
  if (SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    return false;
  }
  return true;
}

bool storage_begin() {
  Lock l;
  if (!mount()) {
    printf("SD: keine Karte gefunden\r\n");
    return false;
  }
  s_ok = true;
  s_ever_ok = true;
  s_last_check = millis();
  const char *dirs[] = { DIR_ROOT, DIR_LOG, DIR_POTS, DIR_RECIPES, DIR_PORTO, DIR_SOUNDS, DIR_SPOOLS, DIR_GAME, DIR_VOICE, DIR_COCKTAILS, DIR_BATT, DIR_MSA };
  for (const char *d : dirs) storage_mkdir(d);
  printf("SD: bereit (%d kHz)\r\n", SD_FREQ_KHZ);
  return true;
}

void storage_loop() {
  uint32_t now = millis();
  if (s_ok) {
    // Karte regelmäßig kurz ansprechen, damit ein Ausfall auch ohne
    // Schreibzugriff bemerkt wird
    if (now - s_last_check > CHECK_EVERY_MS) {
      s_last_check = now;
      Lock l;
      check_card();
    }
    return;
  }
  // Ausgefallen oder beim Start nicht da: regelmäßig neu einhängen
  // Ohne Karte seit dem Start nur selten suchen (Einhängen blockiert kurz)
  if (now - s_last_try < (s_ever_ok ? REMOUNT_EVERY_MS : 30000UL)) return;
  s_last_try = now;
  Lock l;
  SD_MMC.end();
  vTaskDelay(pdMS_TO_TICKS(20));
  if (mount()) {
    s_ok = true;
    s_ever_ok = true;
    s_remounts++;
    s_last_check = now;
    storage_mkdir(DIR_ROOT);
    printf("SD: wieder eingehängt (%lu. Mal)\r\n", (unsigned long)s_remounts);
  }
}

bool storage_ok() {
  return s_ok;
}

unsigned long storage_error_count() {
  return s_errors;
}

int storage_read(const char *path, char *buf, int maxlen) {
  if (!s_ok) return -1;
  Lock l;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    check_card();
    return -1;
  }
  size_t size = f.size();
  if (size > (size_t)(maxlen - 1)) f.seek(size - (maxlen - 1));  // nur das Ende lesen
  int n = f.read((uint8_t *)buf, maxlen - 1);
  f.close();
  if (n < 0) n = 0;
  buf[n] = 0;
  return n;
}

bool storage_write(const char *path, const char *data) {
  if (!s_ok) return false;
  Lock l;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    check_card();
    return false;
  }
  size_t len = strlen(data);
  size_t n = f.print(data);
  f.close();
  if (n != len) {
    check_card();
    return false;
  }
  return true;
}

bool storage_append(const char *path, const char *line) {
  if (!s_ok) return false;
  Lock l;
  File f = SD_MMC.open(path, FILE_APPEND);
  if (!f) {
    check_card();
    return false;
  }
  size_t n = f.print(line);
  n += f.print("\n");
  f.close();
  if (n != strlen(line) + 1) {
    check_card();
    return false;
  }
  return true;
}

bool storage_remove(const char *path) {
  if (!s_ok) return false;
  Lock l;
  return SD_MMC.remove(path);
}

bool storage_exists(const char *path) {
  if (!s_ok) return false;
  Lock l;
  return SD_MMC.exists(path);
}

bool storage_mkdir(const char *path) {
  if (!s_ok) return false;
  Lock l;
  if (SD_MMC.exists(path)) return true;
  return SD_MMC.mkdir(path);
}

static int list(const char *dir, char names[][STORAGE_NAME_LEN], int max, bool dirs) {
  if (!s_ok) return 0;
  Lock l;
  File d = SD_MMC.open(dir);
  if (!d || !d.isDirectory()) {
    if (!d) check_card();
    return 0;
  }
  int n = 0;
  File f = d.openNextFile();
  while (f && n < max) {
    if (f.isDirectory() == dirs) {
      strncpy(names[n], f.name(), STORAGE_NAME_LEN - 1);
      names[n][STORAGE_NAME_LEN - 1] = 0;
      n++;
    }
    f.close();
    f = d.openNextFile();
  }
  d.close();
  return n;
}

int storage_list_dirs(const char *dir, char names[][STORAGE_NAME_LEN], int max) {
  return list(dir, names, max, true);
}

int storage_list(const char *dir, char names[][STORAGE_NAME_LEN], int max) {
  return list(dir, names, max, false);
}
