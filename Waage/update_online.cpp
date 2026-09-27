#include "update_online.h"
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

// Wert zu "key" als Text lesen, ab Position from (GitHub liefert "key": "wert")
static int json_str(const String &js, const char *key, int from, char *out, int len) {
  String k = String("\"") + key + "\"";
  int p = js.indexOf(k, from);
  if (p < 0) return -1;
  p += k.length();
  while (p < (int)js.length() && (js[p] == ' ' || js[p] == ':' || js[p] == '\n' || js[p] == '\t')) p++;
  if (p >= (int)js.length() || js[p] != '"') return -1;
  p++;
  int n = 0;
  while (p < (int)js.length() && js[p] != '"' && n < len - 1) out[n++] = js[p++];
  out[n] = 0;
  return p;
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
static void do_check() {
  s_state = UPD_CHECK;
  WiFiClientSecure cli;
  // Zertifikat wird nicht geprüft (kein Zertifikatsspeicher in der Arduino-IDE).
  // Das Update-Paket selbst prüft der ESP32 beim Schreiben (Prüfsumme).
  cli.setInsecure();
  HTTPClient http;
  String url = String("https://api.github.com/repos/") + OTA_REPO + "/releases/latest";
  if (!http.begin(cli, url)) {
    fail("GitHub nicht erreichbar");
    return;
  }
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent(String("Waage/") + FW_VERSION);
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code == 404) {
    http.end();
    fail("Kein Release gefunden");
    return;
  }
  if (code != 200) {
    printf("Update: GitHub antwortet %d\r\n", code);
    http.end();
    fail("GitHub nicht erreichbar");
    return;
  }
  String js = http.getString();
  http.end();

  char tag[24];
  if (json_str(js, "tag_name", 0, tag, sizeof(tag)) < 0) {
    fail("Kein Release gefunden");
    return;
  }
  const char *t = tag;
  if (*t == 'v' || *t == 'V') t++;
  snprintf(s_latest, sizeof(s_latest), "%s", t);

  // Firmware-Datei unter den Anhängen suchen: bevorzugt Waage.ino.bin,
  // sonst die erste .bin, die kein komplettes Flash-Abbild (merged) ist
  s_url[0] = 0;
  char u[256];
  int p = 0;
  while ((p = json_str(js, "browser_download_url", p, u, sizeof(u))) >= 0) {
    int n = strlen(u);
    bool bin = n > 4 && strcmp(u + n - 4, ".bin") == 0 && !strstr(u, "merged");
    bool exact = n > (int)strlen(OTA_ASSET) && strcmp(u + n - strlen(OTA_ASSET), OTA_ASSET) == 0;
    if (exact || (bin && !s_url[0])) snprintf(s_url, sizeof(s_url), "%s", u);
    if (exact) break;
  }
  if (!s_url[0]) {
    fail("Keine Firmware im Release");
    return;
  }
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
    fail("Download fehlgeschlagen");
    return;
  }
  int len = http.getSize();
  if (len <= 0) {
    http.end();
    fail("Download fehlgeschlagen");
    return;
  }
  if (!Update.begin(len, U_FLASH)) {
    printf("Update: %s\r\n", Update.errorString());
    http.end();
    fail("Zu wenig Platz für das Update");
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
  // WLAN wieder abgeben, wenn es nur für die Suche an war
  if (s_was_task && !s_task && st != UPD_DONE) net_wifi_release();
  s_was_task = s_task;
  if (st == UPD_DONE && millis() - s_done_ms > 3000) ESP.restart();
}
