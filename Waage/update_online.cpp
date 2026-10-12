#include "update_online.h"
#include "disp_rot.h"
#include <esp_heap_caps.h>
#include "config.h"
#include "net.h"
#include "web.h"
#include "sound.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include "i18n.h"
#include "voice.h"
#include "crash.h"
#include "storage.h"
#include "hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WIFI_WAIT_MS 45000   // so lange auf das Heimnetz warten
#define HTTP_TIMEOUT_MS 15000
#define READ_STALL_MS 20000  // Verbindung gilt als weg, wenn so lange nichts kommt
#define DL_TRIES 5           // so oft wird ein abgerissener Download fortgesetzt
#define KEEP_WIFI_MS 180000UL  // nach "neue Version gefunden" das WLAN so lange halten

static volatile upd_state_t s_state = UPD_IDLE;
static volatile int s_progress = 0;
static const char *s_error = "";
static char s_latest[24] = "";
static char s_url[256] = "";
static bool s_rot_paused = false;  // Display-Drehung für das Update ausgesetzt
static bool s_install = false;  // Task soll laden statt suchen
static bool s_task = false;     // Task läuft
static uint32_t s_done_ms = 0;
static upd_state_t s_seen = UPD_IDLE;
static bool s_was_task = false;
static bool s_quiet = false;  // Suche nach dem Start: ohne Töne
static uint32_t s_avail_ms = 0;  // wann die neue Version gefunden wurde

const char *fw_version() {
  return FW_VERSION;
}

bool fw_is_local() {
#ifdef FW_LOCAL
  return true;
#else
  return false;
#endif
}

const char *upd_repo() {
  return OTA_REPO;
}

upd_state_t upd_state() {
  return s_state;
}

bool upd_busy() {
  return s_task;
}

const char *upd_latest() {
  return s_latest;
}

int upd_progress() {
  return s_progress;
}

const char *upd_error() {
  return s_error;
}

static void fail(const char *msg) {
  s_error = msg;
  s_state = UPD_ERROR;
}

// "v1.4.2" / "1.4" -> Zahlen; true, wenn a neuer ist als b
static void ver_parse(const char *s, int v[3]) {
  v[0] = v[1] = v[2] = 0;
  while (*s && (*s < '0' || *s > '9')) s++;  // "v" überspringen
  for (int i = 0; i < 3 && *s; i++) {
    v[i] = atoi(s);
    while (*s >= '0' && *s <= '9') s++;
    if (*s != '.') break;
    s++;
  }
}

static bool ver_newer(const char *a, const char *b) {
  int x[3], y[3];
  ver_parse(a, x);
  ver_parse(b, y);
  for (int i = 0; i < 3; i++)
    if (x[i] != y[i]) return x[i] > y[i];
  return false;
}


static bool wait_wifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  s_state = UPD_CONNECT;
  uint32_t t0 = millis();
  vTaskDelay(pdMS_TO_TICKS(300));  // Verbindungsversuch ist schon angestoßen
  while (millis() - t0 < WIFI_WAIT_MS) {
    if (WiFi.status() == WL_CONNECTED) return true;
    if (!net_connect_busy() && millis() - t0 > 3000) break;  // alle Netze probiert
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  return WiFi.status() == WL_CONNECTED;
}

// ------------------------------------------------------------
//  Neueste Version abfragen
// ------------------------------------------------------------
// Fragt nicht die GitHub-API (Limit 60 Anfragen pro Stunde und große
// JSON-Antwort), sondern nur die Weiterleitung von .../releases/latest:
// GitHub antwortet mit "Location: .../releases/tag/v1.2.3". Das kostet
// fast keinen Speicher und unterliegt keinem API-Limit.
static void do_check() {
  s_state = UPD_CHECK;
  printf("Update: frei intern %u (Block %u)\r\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  int code = 0;
  String loc;
  for (int attempt = 0; attempt < 2; attempt++) {  // ein zweiter Versuch bei Verbindungsfehler
    WiFiClientSecure cli;
    // Zertifikat wird nicht geprüft (kein Zertifikatsspeicher in der Arduino-IDE).
    // Das Update-Paket selbst prüft der ESP32 beim Schreiben (Prüfsumme).
    cli.setInsecure();
    HTTPClient http;
    String url = String("https://github.com/") + OTA_REPO + "/releases/latest";
    if (!http.begin(cli, url)) {
      code = -1;
      continue;
    }
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setUserAgent(String("Waage/") + FW_VERSION);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    const char *keys[] = { "Location" };
    http.collectHeaders(keys, 1);
    code = http.GET();
    loc = http.header("Location");
    http.end();
    if (code > 0) break;
    printf("Update: GitHub %d (%s)\r\n", code, HTTPClient::errorToString(code).c_str());
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
  if (code <= 0) {
    fail("GitHub nicht erreichbar");
    return;
  }
  printf("Update: GitHub %d -> %s\r\n", code, loc.c_str());
  int t = loc.indexOf("/releases/tag/");
  if ((code != 301 && code != 302) || t < 0) {
    fail(code == 404 ? "Repository nicht gefunden" : "Kein Release gefunden");
    return;
  }
  String tag = loc.substring(t + 14);
  int q = tag.indexOf('?');
  if (q >= 0) tag = tag.substring(0, q);
  const char *v = tag.c_str();
  if (*v == 'v' || *v == 'V') v++;
  snprintf(s_latest, sizeof(s_latest), "%s", v);
  // Firmware-Datei des Releases (Name wie im Workflow: Waage.ino.bin)
  snprintf(s_url, sizeof(s_url), "https://github.com/%s/releases/download/%s/%s", OTA_REPO, tag.c_str(), OTA_ASSET);
  printf("Update: installiert %s, auf GitHub %s\r\n", FW_VERSION, s_latest);
  s_state = ver_newer(s_latest, FW_VERSION) ? UPD_AVAILABLE : UPD_LATEST;
}

// ------------------------------------------------------------
//  Laden und schreiben
// ------------------------------------------------------------
// Download in Stücken direkt in die freie Update-Partition. Reißt die
// Verbindung ab (WLAN schwach, GitHub-Server), geht es mit einer
// Range-Anfrage an derselben Stelle weiter statt von vorn.
static void do_install() {
  s_state = UPD_DOWNLOAD;
  s_progress = 0;
  static uint8_t buf[4096];
  int len = -1, got = 0;
  bool begun = false;
  const char *err = "Download fehlgeschlagen";
  crash_phase("Beim Laden des Updates", 0);
  for (int attempt = 0; attempt < DL_TRIES && (len < 0 || got < len); attempt++) {
    if (attempt) {
      printf("Update: Verbindung weg bei %d von %d Byte, setze fort (%d. Versuch)\r\n", got, len, attempt + 1);
      vTaskDelay(pdMS_TO_TICKS(2000));
      if (!wait_wifi()) break;
      s_state = UPD_DOWNLOAD;
    }
    WiFiClientSecure cli;
    cli.setInsecure();
    HTTPClient http;
    if (!http.begin(cli, s_url)) continue;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setUserAgent(String("Waage/") + FW_VERSION);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub leitet auf den Dateispeicher um
    if (got > 0) {
      char r[32];
      snprintf(r, sizeof(r), "bytes=%d-", got);
      http.addHeader("Range", r);  // gilt auch für die Weiterleitung
    }
    int code = http.GET();
    int size = http.getSize();
    if (code == 404) {
      http.end();
      err = "Keine Firmware im Release";
      break;
    }
    if (got > 0 && code == 200) {  // Server kann nicht fortsetzen: von vorn
      printf("Update: kein Fortsetzen möglich, von vorn\r\n");
      Update.abort();
      begun = false;
      got = 0;
    }
    if (!((code == 200 && got == 0) || (code == 206 && got > 0 && size == len - got))) {
      printf("Update: Download %d (%d Byte)\r\n", code, size);
      http.end();
      continue;
    }
    if (!begun) {
      len = size;
      if (len <= 0) {
        http.end();
        break;
      }
      printf("Update: %d Byte, frei intern %u (Block %u), PSRAM %u\r\n", len,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
      if (!Update.begin(len, U_FLASH)) {
        printf("Update: %s\r\n", Update.errorString());
        http.end();
        // Platz in der Update-Partition oder Arbeitsspeicher? Unterschiedliche Meldung
        err = Update.getError() == UPDATE_ERROR_SIZE ? "Zu wenig Platz für das Update" : "Zu wenig Arbeitsspeicher, bitte neu starten";
        len = -1;
        break;
      }
      begun = true;
    }
    WiFiClient *st = http.getStreamPtr();
    uint32_t last = millis();
    int last_pct = -1;
    bool write_err = false;
    while (got < len) {
      int avail = st->available();
      if (avail > 0) {
        int n = st->readBytes(buf, avail > (int)sizeof(buf) ? sizeof(buf) : avail);
        if (n > 0) {
          if (Update.write(buf, n) != (size_t)n) {
            write_err = true;
            break;
          }
          got += n;
          s_progress = (int)((int64_t)got * 100 / len);
          if (s_progress / 5 != last_pct) {  // für den Absturzbericht: wie weit es kam
            last_pct = s_progress / 5;
            crash_phase("Beim Laden des Updates", s_progress);
          }
          last = millis();
        }
        vTaskDelay(1);  // Leerlauf-Task auf Kern 0 drankommen lassen (Watchdog)
      } else {
        if (!st->connected() || millis() - last > READ_STALL_MS) break;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }
    http.end();
    if (write_err) {
      printf("Update: Schreiben: %s\r\n", Update.errorString());
      err = "Update fehlgeschlagen";
      break;
    }
  }
  if (!begun || len <= 0 || got < len) {
    if (begun) Update.abort();
    crash_phase(NULL, 0);
    fail(err);
    return;
  }
  crash_phase("Beim Prüfen des Updates", 100);
  if (!Update.end(true)) {  // prüft das Abbild und stellt die neue Partition ein
    printf("Update: %s\r\n", Update.errorString());
    crash_phase(NULL, 0);
    fail("Update fehlgeschlagen");
    return;
  }
  s_progress = 100;
  s_state = UPD_DONE;
}

static void task(void *arg) {
  if (!s_install) crash_phase("Bei der Update-Suche", 0);
  if (!wait_wifi()) fail("Kein WLAN");
  else if (s_install) do_install();
  else do_check();
  if (s_state != UPD_DONE) crash_phase(NULL, 0);
  if (s_state == UPD_AVAILABLE) s_avail_ms = millis();
  printf("Update: Stapel frei %u Byte\r\n", (unsigned)uxTaskGetStackHighWaterMark(NULL));
  s_task = false;
  vTaskDelete(NULL);
}

static void start(bool install) {
  if (s_task) return;
  if (strstr(OTA_REPO, "DEIN-GITHUB-NAME")) {  // noch nicht eingerichtet
    fail("Repository in config.h eintragen");
    return;
  }
  if (!net_has_credentials() && WiFi.status() != WL_CONNECTED) {
    fail("Kein WLAN gespeichert");
    return;
  }
  s_install = install;
  s_error = "";
  // Spracherkennung ruht: Mikrofon und Netz brauchen Rechenzeit auf Kern 0
  // (dort laufen auch WLAN und TLS), wieder an in upd_loop
  voice_hold(true);
  // Verbindung anstoßen (eigenes WLAN der Weboberfläche dabei offen lassen)
  if (WiFi.status() != WL_CONNECTED) net_connect_start(web_running() && web_ap_mode());
  // Während Suche und Download das Display-Drehen aussetzen: TLS und WLAN
  // brauchen den internen Speicher und die Rechenzeit (wird in upd_loop
  // wieder eingeschaltet). Alle Aufrufer laufen im LVGL-Kontext.
  if (disp_rot_get() != 0) {
    disp_rot_suspend();
    s_rot_paused = true;
  }
  s_task = true;
  s_state = UPD_CONNECT;
  // TLS braucht Stapel; auf Kern 0 neben dem WLAN, die Anzeige läuft weiter
  if (xTaskCreatePinnedToCore(task, "update", 12288, NULL, 1, NULL, 0) != pdPASS) {
    s_task = false;
    fail("Zu wenig Speicher");
  }
}

bool upd_wants_wifi() {
  if (s_task) return true;
  // neue Version gefunden: WLAN noch halten, gleich wird wohl installiert
  return s_state == UPD_AVAILABLE && millis() - s_avail_ms < KEEP_WIFI_MS;
}

// Sauber neu starten: beim Router abmelden (sonst lehnt er die erste
// Wiederanmeldung nach dem Neustart manchmal ab), SD-Karte aushängen,
// Display in den Reset (hal_restart)
void upd_restart() {
  crash_phase(NULL, 0);
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true);
    delay(150);
    WiFi.mode(WIFI_OFF);
  }
  storage_end();
  hal_restart();
}

void upd_check() {
  s_quiet = false;
  start(false);
}

void upd_check_quiet() {
  s_quiet = true;
  start(false);
}

void upd_install() {
  s_quiet = false;
  if (s_state == UPD_AVAILABLE && s_url[0]) start(true);
}

void upd_loop() {
  upd_state_t st = s_state;
  if (st != s_seen) {
    s_seen = st;

    if (!s_quiet) {
      if (st == UPD_AVAILABLE || st == UPD_LATEST) sound_play(SND_CLICK);
      if (st == UPD_ERROR) sound_play(SND_WARN);
    }
    if (st == UPD_DONE) {
      sound_play(SND_DONE);
      s_done_ms = millis();
      // Für den nächsten Start merken, welche Version kommen soll (siehe upd_boot_notice)
      Preferences p;
      p.begin("waage", false);
      p.putString("upd_to", s_latest);
      p.end();
    }
  }
  // Display-Drehung und Spracherkennung nach Suche oder Fehler wieder
  // einschalten (bei Erfolg startet die Waage neu)
  if (!s_task && st != UPD_DONE) {
    if (s_rot_paused) {
      disp_rot_resume();
      s_rot_paused = false;
    }
    voice_hold(false);
  }
  // WLAN wieder abgeben, wenn es nur für die Suche an war. Ist eine neue
  // Version da, bleibt es noch ein paar Minuten an (Installieren folgt meist).
  static bool s_release = false;
  if (s_was_task && !s_task && st != UPD_DONE) s_release = true;
  if (s_release && !s_task && !upd_wants_wifi()) {
    s_release = false;
    net_wifi_release();
  }
  s_was_task = s_task;
  if (st == UPD_DONE && millis() - s_done_ms > 3000) upd_restart();
}

// ------------------------------------------------------------
//  OTA-Rollback
//  Das Boardpaket markiert eine neue Firmware sonst schon beim Start als
//  gültig. Mit verifyRollbackLater() = true übernimmt das upd_mark_valid(),
//  das die Oberfläche erst nach einigen Sekunden sauberem Lauf aufruft.
//  Hängt oder startet die neue Firmware vorher neu, schaltet der Bootloader
//  beim nächsten Start auf die vorige Version zurück.
// ------------------------------------------------------------
extern "C" bool verifyRollbackLater() {
  return true;
}

void upd_mark_valid() {
  static bool done = false;
  if (done) return;
  done = true;
  const esp_partition_t *run = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    printf("Update: neue Firmware %s als gültig markiert\r\n", FW_VERSION);
  }
}

bool upd_pending_verify() {
  const esp_partition_t *run = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  return run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
}

const char *upd_boot_notice(bool *ok) {
  static char b[96];
  char to[24] = "";
  Preferences p;
  p.begin("waage", false);
  p.getString("upd_to", to, sizeof(to));
  if (to[0]) p.remove("upd_to");
  p.end();
  if (!to[0]) return NULL;
  int x[3], y[3];
  ver_parse(to, x);
  ver_parse(FW_VERSION, y);
  *ok = x[0] == y[0] && x[1] == y[1] && x[2] == y[2];
  if (*ok) snprintf(b, sizeof(b), T("Aktualisiert auf Version %s"), FW_VERSION);
  else snprintf(b, sizeof(b), "%s", T("Update fehlgeschlagen, alte Version aktiv"));
  return b;
}
