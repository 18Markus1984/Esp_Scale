#pragma once
// ============================================================
//  WLAN + Uhrzeit per NTP
//  Die Waage verbindet sich nur kurz: Verbinden -> Zeit holen ->
//  in den RTC schreiben -> WLAN wieder aus. Danach einmal täglich.
// ============================================================
#include <stdint.h>

typedef enum {
  NET_OFF,         // nichts eingerichtet / WLAN aus
  NET_CONNECTING,
  NET_SYNCING,     // verbunden, wartet auf NTP
  NET_OK,          // Zeit erfolgreich abgeglichen
  NET_FAIL         // Verbindung oder Abgleich fehlgeschlagen
} net_state_t;

typedef struct {
  char ssid[33];
  int rssi;
  bool open;
} net_ap_t;

void net_begin();                 // beim Start: abgleichen, falls eingerichtet
void net_loop();                  // regelmäßig aufrufen (z. B. alle 100 ms)
void net_sync_now();
net_state_t net_state();
bool net_has_credentials();       // mindestens ein Netz gespeichert?
void net_set_credentials(const char *ssid, const char *pass);  // speichert (oder aktualisiert) + gleicht ab

// ---- Mehrere gespeicherte Netze ----
// Bis zu NET_MAX Netze (Flash, Bereich "wlan"). Beim Verbinden sucht die
// Waage zuerst, welche davon in Reichweite sind, und nimmt das stärkste;
// klappt es nicht, das nächste. g_set.ssid ist das zuletzt verbundene.
#define NET_MAX 5
int         net_count();
const char *net_ssid(int i);
void        net_forget(int i);
void        net_store(const char *ssid, const char *pass);  // nur speichern, Verbindung bleibt
// Verbindung mit einem bekannten Netz aufbauen (nicht blockierend).
// keep_ap = eigenes WLAN der Waage dabei weiterlaufen lassen
void        net_connect_start(bool keep_ap);
bool        net_connect_busy();
void        net_wifi_release();  // WLAN aus, wenn niemand es mehr braucht (Uhrzeit, Weboberfläche)     // sucht oder probiert gerade
uint32_t net_last_sync_ms();      // 0 = noch nie

void net_scan_start();
int  net_scan_get(net_ap_t *out, int max);  // -1 = läuft noch, sonst Anzahl
