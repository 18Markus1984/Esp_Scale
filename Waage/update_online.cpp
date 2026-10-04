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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WIFI_WAIT_MS 45000   // so lange auf das Heimnetz warten
#define HTTP_TIMEOUT_MS 15000
#define READ_STALL_MS 20000  // Download bricht ab, wenn so lange nichts kommt

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
static void do_install() {
  s_state = UPD_DOWNLOAD;
  s_progress = 0;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  if (!http.begin(cli, s_url)) {
    fail("Download fehlgeschlagen");
    return;
  }
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent(String("Waage/") + FW_VERSION);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub leitet auf den Dateispeicher um
  int code = http.GET();
  if (code != 200) {
    printf("Update: Download %d\r\n", code);
    http.end();
    fail(code == 404 ? "Keine Firmware im Release" : "Download fehlgeschlagen");
    return;
  }
  int len = http.getSize();
  if (len <= 0) {
    http.end();
    fail("Download fehlgeschlagen");
    return;
  }
  printf("Update: %d Byte, frei intern %u (Block %u), PSRAM %u\r\n", len,
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  if (!Update.begin(len, U_FLASH)) {
    printf("Update: %s\r\n", Update.errorString());
    http.end();
    // Platz in der Update-Partition oder Arbeitsspeicher? Unterschiedliche Meldung
    fail(Update.getError() == UPDATE_ERROR_SIZE ? "Zu wenig Platz für das Update" : "Zu wenig Arbeitsspeicher, bitte neu starten");
    return;
  }
  WiFiClient *st = http.getStreamPtr();
  static uint8_t buf[4096];
  int got = 0;
  uint32_t last = millis();
  while (got < len) {
    int avail = st->available();
    if (avail > 0) {
      int n = st->readBytes(buf, avail > (int)sizeof(buf) ? sizeof(buf) : avail);
      if (n > 0) {
        if (Update.write(buf, n) != (size_t)n) break;
        got += n;
        s_progress = (int)((int64_t)got * 100 / len);
        last = millis();
      }
    } else {
      if (!st->connected() || millis() - last > READ_STALL_MS) break;
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  http.end();
  if (got < len) {
    Update.abort();
    fail("Download fehlgeschlagen");
    return;
  }
  if (!Update.end(true)) {  // prüft das Abbild und stellt die neue Partition ein
    printf("Update: %s\r\n", Update.errorString());
    fail("Update fehlgeschlagen");
    return;
  }
  s_progress = 100;
  s_state = UPD_DONE;
}

static void task(void *arg) {
  if (!wait_wifi()) fail("Kein WLAN");
  else if (s_install) do_install();
  else do_check();
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

void upd_check() {
  start(false);
}

void upd_install() {
  if (s_state == UPD_AVAILABLE && s_url[0]) start(true);
}

void upd_loop() {
  upd_state_t st = s_state;
  if (st != s_seen) {
    s_seen = st;

    if (st == UPD_AVAILABLE || st == UPD_LATEST) sound_play(SND_CLICK);
    if (st == UPD_ERROR) sound_play(SND_WARN);
    if (st == UPD_DONE) {
      sound_play(SND_DONE);
      s_done_ms = millis();
    }
  }
  // Display-Drehung nach Suche oder Fehler wieder einschalten (bei Erfolg startet die Waage neu)
  if (s_rot_paused && !s_task && st != UPD_DONE) {
    disp_rot_resume();
    s_rot_paused = false;
  }
  // WLAN wieder abgeben, wenn es nur für die Suche an war
  if (s_was_task && !s_task && st != UPD_DONE) net_wifi_release();
  s_was_task = s_task;
  if (st == UPD_DONE && millis() - s_done_ms > 3000) ESP.restart();
}
