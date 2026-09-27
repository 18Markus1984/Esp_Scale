#include "net.h"
#include "settings.h"
#include "hal.h"
#include "web.h"
#include "update_online.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <string.h>

// Zeitzone Deutschland inkl. Sommerzeit
#define TZ_DE "CET-1CEST,M3.5.0,M10.5.0/3"
#define CONNECT_TIMEOUT_MS 20000
#define NTP_TIMEOUT_MS 15000
#define RESYNC_MS (24UL * 3600UL * 1000UL)
#define RETRY_MS (30UL * 60UL * 1000UL)

static net_state_t s_state = NET_OFF;
static uint32_t s_t0 = 0;
static uint32_t s_last_sync = 0;
static bool s_scanning = false;

// ------------------------------------------------------------
//  Liste der gespeicherten Netze
// ------------------------------------------------------------
static char s_ss[NET_MAX][33];
static char s_pw[NET_MAX][65];
static int s_n = 0;

static void list_save() {
  Preferences p;
  p.begin("wlan", false);
  p.putInt("n", s_n);
  for (int i = 0; i < NET_MAX; i++) {
    char ks[4], kp[4];
    snprintf(ks, sizeof(ks), "s%d", i);
    snprintf(kp, sizeof(kp), "p%d", i);
    if (i < s_n) {
      p.putString(ks, s_ss[i]);
      p.putString(kp, s_pw[i]);
    } else {
      p.remove(ks);
      p.remove(kp);
    }
  }
  p.end();
}

static void list_load() {
  Preferences p;
  p.begin("wlan", true);
  s_n = p.getInt("n", 0);
  if (s_n < 0 || s_n > NET_MAX) s_n = 0;
  for (int i = 0; i < s_n; i++) {
    char ks[4], kp[4];
    snprintf(ks, sizeof(ks), "s%d", i);
    snprintf(kp, sizeof(kp), "p%d", i);
    memset(s_ss[i], 0, sizeof(s_ss[i]));
    memset(s_pw[i], 0, sizeof(s_pw[i]));
    p.getString(ks, s_ss[i], sizeof(s_ss[i]));
    p.getString(kp, s_pw[i], sizeof(s_pw[i]));
  }
  p.end();
  // Umstieg von der alten Einstellung mit nur einem Netz
  if (s_n == 0 && g_set.ssid[0]) {
    snprintf(s_ss[0], sizeof(s_ss[0]), "%s", g_set.ssid);
    snprintf(s_pw[0], sizeof(s_pw[0]), "%s", g_set.pass);
    s_n = 1;
    list_save();
  }
}

int net_count() {
  return s_n;
}

const char *net_ssid(int i) {
  return (i >= 0 && i < s_n) ? s_ss[i] : "";
}

void net_forget(int i) {
  if (i < 0 || i >= s_n) return;
  bool was_current = strcmp(s_ss[i], g_set.ssid) == 0;
  for (int k = i; k < s_n - 1; k++) {
    memcpy(s_ss[k], s_ss[k + 1], sizeof(s_ss[k]));
    memcpy(s_pw[k], s_pw[k + 1], sizeof(s_pw[k]));
  }
  s_n--;
  list_save();
  if (was_current) {  // Anzeige "Netz: …" auf das nächste zeigen lassen
    snprintf(g_set.ssid, sizeof(g_set.ssid), "%s", s_n ? s_ss[0] : "");
    snprintf(g_set.pass, sizeof(g_set.pass), "%s", s_n ? s_pw[0] : "");
    settings_save();
  }
}

// ------------------------------------------------------------
//  Verbinden: bekannte Netze in Reichweite suchen, stärkstes zuerst
// ------------------------------------------------------------
#define TRY_MS 10000      // so lange je Netz probieren
#define SCAN_MAX_MS 8000
static int c_phase = 0;   // 0 = nichts, 1 = sucht, 2 = probiert Netz c_cand[c_i]
static int c_cand[NET_MAX], c_nc = 0, c_i = 0;
static uint32_t c_t0 = 0;

static void cand_all() {  // alle in gespeicherter Reihenfolge (z. B. verstecktes Netz)
  c_nc = s_n;
  for (int i = 0; i < s_n; i++) c_cand[i] = i;
}

static void cand_begin() {
  int k = c_cand[c_i];
  WiFi.disconnect();
  WiFi.begin(s_ss[k], s_pw[k]);
  c_phase = 2;
  c_t0 = millis();
}

void net_connect_start(bool keep_ap) {
  if (s_n == 0) return;
  WiFi.mode(keep_ap ? WIFI_AP_STA : WIFI_STA);
  c_i = 0;
  if (s_n == 1 || s_scanning) {  // nur eins, oder die Netzsuche im Setup belegt den Scanner
    cand_all();
    cand_begin();
    return;
  }
  WiFi.scanDelete();
  WiFi.scanNetworks(true);  // asynchron
  c_phase = 1;
  c_t0 = millis();
}

bool net_connect_busy() {
  return c_phase != 0;
}

static void connect_tick() {
  if (c_phase == 1) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING && millis() - c_t0 < SCAN_MAX_MS) return;
    int rssi[NET_MAX];
    c_nc = 0;
    for (int k = 0; k < s_n; k++) {
      rssi[k] = -999;
      for (int i = 0; i < n; i++)
        if (WiFi.SSID(i) == s_ss[k] && WiFi.RSSI(i) > rssi[k]) rssi[k] = WiFi.RSSI(i);
    }
    // gefundene Netze nach Stärke, danach die übrigen (vielleicht versteckt)
    for (int pass = 0; pass < 2; pass++)
      for (int k = 0; k < s_n; k++) {
        bool found = rssi[k] > -999;
        if ((pass == 0) != found) continue;
        int j = c_nc++;
        while (pass == 0 && j > 0 && rssi[c_cand[j - 1]] < rssi[k]) {
          c_cand[j] = c_cand[j - 1];
          j--;
        }
        c_cand[j] = k;
      }
    WiFi.scanDelete();
    c_i = 0;
    printf("WLAN: %d gespeichert, bestes: %s\r\n", s_n, s_ss[c_cand[0]]);
    cand_begin();
  } else if (c_phase == 2) {
    if (WiFi.status() == WL_CONNECTED) {
      c_phase = 0;
      int k = c_cand[c_i];
      if (strcmp(g_set.ssid, s_ss[k]) != 0) {  // zuletzt verbundenes merken (für die Anzeige)
        snprintf(g_set.ssid, sizeof(g_set.ssid), "%s", s_ss[k]);
        snprintf(g_set.pass, sizeof(g_set.pass), "%s", s_pw[k]);
        settings_save();
      }
    } else if (millis() - c_t0 > TRY_MS) {
      if (++c_i < c_nc) cand_begin();
      else c_phase = 0;  // keins erreichbar
    }
  }
}

static void wifi_off() {
  if (s_scanning) return;  // Suche läuft noch
  if (upd_busy()) return;   // Online-Update braucht das WLAN
  if (web_running()) return;  // Weboberfläche braucht das WLAN noch
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void net_begin() {
  list_load();
  if (net_has_credentials()) net_sync_now();
}

bool net_has_credentials() {
  return s_n > 0;
}

void net_sync_now() {
  if (!net_has_credentials()) return;
  if (upd_busy()) return;  // Online-Update läuft, Verbindung nicht umschalten
  if (web_running()) {
    // WLAN gehört gerade der Weboberfläche: nicht umschalten.
    // Ist sie im Heimnetz verbunden, die Uhrzeit einfach mitholen.
    if (!web_ap_mode() && WiFi.status() == WL_CONNECTED) {
      configTzTime(TZ_DE, "pool.ntp.org", "time.nist.gov");
      s_state = NET_SYNCING;
    } else {
      s_state = NET_FAIL;  // später erneut
    }
    s_t0 = millis();
    return;
  }
  net_connect_start(false);
  s_state = NET_CONNECTING;
  s_t0 = millis();
}

void net_set_credentials(const char *ssid, const char *pass) {
  net_store(ssid, pass);
  strncpy(g_set.ssid, ssid, sizeof(g_set.ssid) - 1);
  g_set.ssid[sizeof(g_set.ssid) - 1] = 0;
  strncpy(g_set.pass, pass ? pass : "", sizeof(g_set.pass) - 1);
  g_set.pass[sizeof(g_set.pass) - 1] = 0;
  settings_save();
  WiFi.disconnect(true);
  net_sync_now();
}

void net_store(const char *ssid, const char *pass) {
  if (!ssid || !ssid[0]) return;
  // schon bekannt? Dann entfernen und vorne neu einsetzen (neues Passwort)
  for (int i = 0; i < s_n; i++)
    if (strcmp(s_ss[i], ssid) == 0) {
      for (int k = i; k < s_n - 1; k++) {
        memcpy(s_ss[k], s_ss[k + 1], sizeof(s_ss[k]));
        memcpy(s_pw[k], s_pw[k + 1], sizeof(s_pw[k]));
      }
      s_n--;
      break;
    }
  if (s_n == NET_MAX) s_n--;  // voll: das älteste fällt heraus
  for (int k = s_n; k > 0; k--) {
    memcpy(s_ss[k], s_ss[k - 1], sizeof(s_ss[k]));
    memcpy(s_pw[k], s_pw[k - 1], sizeof(s_pw[k]));
  }
  snprintf(s_ss[0], sizeof(s_ss[0]), "%s", ssid);
  snprintf(s_pw[0], sizeof(s_pw[0]), "%s", pass ? pass : "");
  s_n++;
  list_save();
}

void net_wifi_release() {
  if (s_state == NET_CONNECTING || s_state == NET_SYNCING || net_connect_busy()) return;
  wifi_off();  // bleibt an, wenn die Weboberfläche läuft
}

net_state_t net_state() {
  return s_state;
}

uint32_t net_last_sync_ms() {
  return s_last_sync;
}

void net_loop() {
  connect_tick();  // auch für die Weboberfläche
  uint32_t now = millis();
  switch (s_state) {
    case NET_CONNECTING:
      if (WiFi.status() == WL_CONNECTED && !net_connect_busy()) {
        configTzTime(TZ_DE, "pool.ntp.org", "time.nist.gov");
        s_state = NET_SYNCING;
        s_t0 = now;
      } else if (!net_connect_busy() || now - s_t0 > CONNECT_TIMEOUT_MS + NET_MAX * TRY_MS) {
        c_phase = 0;
        s_state = NET_FAIL;
        s_t0 = now;
        wifi_off();
      }
      break;

    case NET_SYNCING: {
      struct tm t;
      if (getLocalTime(&t, 0) && t.tm_year + 1900 >= 2024) {
        hal_set_datetime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        s_last_sync = now ? now : 1;
        s_state = NET_OK;
        wifi_off();
      } else if (now - s_t0 > NTP_TIMEOUT_MS) {
        s_state = NET_FAIL;
        s_t0 = now;
        wifi_off();
      }
      break;
    }

    case NET_OK:
      if (now - s_last_sync > RESYNC_MS) net_sync_now();
      break;

    case NET_FAIL:
      if (now - s_t0 > RETRY_MS) net_sync_now();
      break;

    default:
      break;
  }
}

void net_scan_start() {
  if (s_scanning) return;
  if (c_phase == 1) {  // Verbindungssuche läuft: abbrechen, die Netzsuche hat Vorrang
    WiFi.scanDelete();
    c_phase = 0;
  }
  // eigenes WLAN der Weboberfläche nicht abschalten
  if (web_running() && web_ap_mode()) WiFi.mode(WIFI_AP_STA);
  else if (s_state != NET_CONNECTING && s_state != NET_SYNCING) WiFi.mode(WIFI_STA);
  WiFi.scanNetworks(true);  // asynchron
  s_scanning = true;
}

int net_scan_get(net_ap_t *out, int max) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return -1;
  int k = 0;
  for (int i = 0; i < n && k < max; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    bool dup = false;  // gleiche Netze (mehrere Router) nur einmal
    for (int j = 0; j < k; j++)
      if (strcmp(out[j].ssid, ssid.c_str()) == 0) dup = true;
    if (dup) continue;
    strncpy(out[k].ssid, ssid.c_str(), sizeof(out[k].ssid) - 1);
    out[k].ssid[sizeof(out[k].ssid) - 1] = 0;
    out[k].rssi = WiFi.RSSI(i);
    out[k].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    k++;
  }
  // stärkstes Netz zuerst
  for (int i = 1; i < k; i++) {
    net_ap_t t = out[i];
    int j = i - 1;
    while (j >= 0 && out[j].rssi < t.rssi) { out[j + 1] = out[j]; j--; }
    out[j + 1] = t;
  }
  WiFi.scanDelete();
  s_scanning = false;
  if (s_state != NET_CONNECTING && s_state != NET_SYNCING) wifi_off();
  return k;
}
