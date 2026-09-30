// ============================================================
//  Weboberfläche: Server und Endpunkte
//  Die Seite selbst steht in web_page.h (erzeugt aus web/page.html).
//  Alle Endpunkte laufen im UI-Takt (web_loop), deshalb dürfen sie
//  direkt auf Waage, SD-Karte und Einstellungen zugreifen.
// ============================================================
#include "web.h"
#include "web_page.h"
#include "settings.h"
#include "storage.h"
#include "data.h"
#include "scale.h"
#include "hal.h"
#include "sound.h"
#include "net.h"
#include "ble_kbd.h"
#include "batt.h"
#include "ui.h"
#include "config.h"
#include "tools.h"
#include "update_online.h"
#include "refcheck.h"
#include "voice.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <FS.h>
#include <SD_MMC.h>

#define AP_SSID "Waage-Setup"
#define IDLE_OFF_MS (10UL * 60UL * 1000UL)  // 10 min ohne Zugriff -> aus
#define CONNECT_TIMEOUT_MS 12000

static WebServer s_server(80);
static DNSServer s_dns;  // Captive Portal im eigenen WLAN
static bool s_running = false;
static bool s_ap = false;
static bool s_connecting = false;
static bool s_routes = false;
static uint32_t s_last_hit = 0;
static uint32_t s_start_ms = 0;
static char s_url[40] = "";
static bool s_ota = false;
static uint32_t s_ota_until = 0;  // Update-Modus: bis dahin nicht abschalten      // ArduinoOTA läuft (nur im Heimnetz sinnvoll)
static bool s_update_ok = false;  // letzte Firmware-Übertragung erfolgreich?
static float s_park_goal = 0;   // Parkpiepser für Ziel/Rezept im Browser
static bool s_hide = false;     // Gewicht am Display verstecken (Schätzspiel)
static bool s_portal_ok = false; // Seite wurde im eigenen WLAN schon geöffnet
static char s_ip[20] = "";

// ------------------------------------------------------------
//  Hilfen
// ------------------------------------------------------------
static void hit() {
  s_last_hit = millis();
}

static void send_json(const String &s) {
  hit();
  s_server.send(200, "application/json", s);
}

static void ok(bool good = true, const char *msg = "") {
  String s = good ? "{\"ok\":1" : "{\"ok\":0";
  if (msg[0]) s += String(",\"msg\":\"") + msg + "\"";
  send_json(s + "}");
}

static String esc(const char *s) {  // für JSON-Strings
  String o;
  for (const char *p = s; *p; p++) {
    if (*p == '"' || *p == '\\') o += '\\';
    if ((unsigned char)*p >= 0x20) o += *p;
  }
  return o;
}

// sehr einfacher JSON-Leser: sucht "key": und liest Zahl, true/false oder String
static String jval(const String &src, const String &key, int from = 0) {
  int i = src.indexOf("\"" + key + "\":", from);
  if (i < 0) return "";
  i += key.length() + 3;
  while (i < (int)src.length() && src[i] == ' ') i++;
  if (src[i] == '"') {
    int e = i + 1;
    while (e < (int)src.length() && !(src[e] == '"' && src[e - 1] != '\\')) e++;
    return src.substring(i + 1, e);
  }
  int e = i;
  while (e < (int)src.length() && src[e] != ',' && src[e] != '}' && src[e] != ']') e++;
  return src.substring(i, e);
}

// JSON-Escapes (\" \\ \/) auflösen, z. B. für Passwörter mit Sonderzeichen
static String junesc(const String &v) {
  String out;
  for (int i = 0; i < (int)v.length(); i++) {
    char c = v[i];
    if (c == '\\' && i + 1 < (int)v.length()) {
      char n = v[++i];
      out += (n == 'n') ? '\n' : (n == 't') ? '\t' : n;
    } else {
      out += c;
    }
  }
  return out;
}

// Objekte {..} einer Liste "key":[...] nacheinander liefern
static int jnext(const String &src, const String &key, int pos, String &item) {
  int list = src.indexOf("\"" + key + "\"");
  if (list < 0) return -1;
  int end = src.indexOf("]", list);
  if (pos < list) pos = list;
  int i = src.indexOf("{", pos + 1);
  if (i < 0 || i > end) return -1;
  int e = src.indexOf("}", i);
  item = src.substring(i, e + 1);
  return e;
}

// ------------------------------------------------------------
//  Übersicht, Wiegen, Töne
// ------------------------------------------------------------
static void h_root() {
  hit();
  s_portal_ok = true;
  s_server.sendHeader("Content-Encoding", "gzip");
  s_server.sendHeader("Cache-Control", "max-age=30");
  s_server.send_P(200, "text/html; charset=utf-8", (const char *)WEB_PAGE_GZ, WEB_PAGE_LEN);
}

static void h_state() {
  char disp[16];
  int u = g_set.unit;
  float net = scale_net();
  unit_fmt(disp, sizeof(disp), unit_from_g(scale_overload() ? scale_gross() : net, u), u);
  int h = 0, m = 0;
  hal_time(&h, &m);
  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", h, m);
  String s = "{\"g\":" + String(net, 1) + ",\"gross\":" + String(scale_gross(), 1) + ",\"disp\":\"" + disp +
             "\",\"unit\":\"" + unit_name(u) + "\",\"stable\":" + (scale_stable() ? "true" : "false") +
             ",\"over\":" + (scale_overload() ? "true" : "false") + ",\"pot\":\"" + esc(ui_active_pot()) +
             "\",\"off\":" + web_seconds_left() + ",\"time\":\"" + t + "\",\"bat\":" + hal_battery_percent() +
             ",\"ble\":" + (ble_kbd_connected() ? "true" : "false") + ",\"max\":" + String((int)WAAGE_MAX_G);
  // Hintergrunddienste: nächster Timer (s, -1 = keiner), klingelt einer?, Langzeitmessung
  int tn = timer_next();
  bool ring = false;
  for (int i = 0; i < TIMER_COUNT; i++) ring = ring || timer_ringing(i);
  s += ",\"tm\":" + String(tn >= 0 ? timer_left(tn) : -1) + ",\"ring\":" + (ring ? "true" : "false") +
       ",\"lt\":" + (lt_running() ? "true" : "false") + "}";
  send_json(s);
}

static void h_tara() {
  scale_tare();
  sound_play(SND_TARA);
  ok();
}

static void h_save() {
  String note = s_server.hasArg("note") ? s_server.arg("note") : String(ui_active_pot());
  int r = log_add(scale_net(), note.c_str());
  sound_play(r == 0 ? SND_WARN : SND_SAVE);
  ok(r != 0);
}

static void h_log_add() {  // Ergebnisse der Modi im Browser (Rezept, Ziel, Spule)
  int r = log_add(s_server.arg("g").toFloat(), s_server.arg("note").c_str());
  ok(r != 0);
}

static void h_park() {
  s_park_goal = s_server.arg("goal").toFloat();
  sound_parking_reset();
  ok();
}

static void h_sound() {
  String s = s_server.arg("s");
  if (s == "drum") sound_play(SND_DRUM);
  else if (s == "done") sound_play(SND_DONE);
  else if (s == "save") sound_play(SND_SAVE);
  else if (s == "warn") sound_play(SND_WARN);
  else if (s == "reached") sound_play(SND_REACHED);
  else if (s == "tick") sound_play(SND_TICK);
  else sound_play(SND_TARA);
  ok();
}

static void h_hide() {
  s_hide = s_server.arg("on") == "1";
  ok();
}

static void h_ble_send() {
  ble_kbd_begin();
  bool good = ble_kbd_send_line(s_server.arg("n").c_str());
  if (good) sound_play(SND_SAVE);
  ok(good);
}

static void h_players() {
  players_load();
  String s = "[";
  for (int i = 0; i < g_player_count; i++) {
    if (i) s += ",";
    s += "\"" + esc(player_name(i)) + "\"";
  }
  send_json(s + "]");
}

static void h_players_save() {  // Body: {"names":["Lena","Tom"]}
  String b = s_server.arg("plain");
  int i = b.indexOf("[");
  int n = 0;
  memset(g_player_names, 0, sizeof(g_player_names));
  while (i >= 0 && n < PLAYER_MAX) {
    int a = b.indexOf('"', i + 1);
    if (a < 0) break;
    int e = b.indexOf('"', a + 1);
    if (e < 0) break;
    strncpy(g_player_names[n], b.substring(a + 1, e).c_str(), sizeof(g_player_names[0]) - 1);
    n++;
    i = e;
    if (b.indexOf(']', i) < b.indexOf('"', i + 1) || b.indexOf('"', i + 1) < 0) break;
  }
  if (n >= 2) g_player_count = n;
  ok(players_save());
}

static void h_game() {
  String line = jval(s_server.arg("plain"), "line");
  ok(storage_append(DIR_GAME "/spiele.txt", line.c_str()));
}

// ------------------------------------------------------------
//  Rezepte
// ------------------------------------------------------------
// Ordner aus dem Parameter: ?dir=cocktails -> Cocktails, sonst Rezepte
static const char *arg_dir() {
  return s_server.arg("dir") == "cocktails" ? DIR_COCKTAILS : DIR_RECIPES;
}

static void h_recipes() {
  static char files[20][48], names[20][40];
  static int counts[20];
  int n = recipes_list_dir(arg_dir(), files, names, counts, 20);
  String s = "[";
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    s += "{\"file\":\"" + esc(files[i]) + "\",\"name\":\"" + esc(names[i]) + "\",\"count\":" + counts[i] + "}";
  }
  send_json(s + "]");
}

static void h_recipe() {
  static recipe_t r;
  if (!recipe_load_dir(arg_dir(), s_server.arg("file").c_str(), &r)) {
    send_json("{\"file\":\"\",\"name\":\"\",\"portions\":2,\"ing\":[]}");
    return;
  }
  String s = "{\"file\":\"" + esc(r.file) + "\",\"name\":\"" + esc(r.name) + "\",\"portions\":" + r.portions + ",\"ing\":[";
  for (int i = 0; i < r.count; i++) {
    if (i) s += ",";
    s += "{\"name\":\"" + esc(r.ing[i].name) + "\",\"grams\":" + String(r.ing[i].grams, 1) + "}";
  }
  send_json(s + "]}");
}

static void h_recipe_save() {
  String body = s_server.arg("plain");
  static recipe_t r;
  memset(&r, 0, sizeof(r));
  strncpy(r.name, jval(body, "name").c_str(), sizeof(r.name) - 1);
  r.portions = jval(body, "portions").toInt();
  if (r.portions < 1) r.portions = 1;
  String file = jval(body, "file");
  String item;
  int pos = 0;
  while (r.count < RECIPE_MAX_ING && (pos = jnext(body, "ing", pos, item)) >= 0) {
    strncpy(r.ing[r.count].name, jval(item, "name").c_str(), sizeof(r.ing[0].name) - 1);
    r.ing[r.count].grams = jval(item, "grams").toFloat();
    if (r.ing[r.count].name[0] && r.ing[r.count].grams > 0) r.count++;
  }
  if (!r.count || !r.name[0]) {
    ok(false, "Name und Zutaten fehlen");
    return;
  }
  if (file.length()) recipe_delete_dir(arg_dir(), file.c_str());  // bestehendes Rezept ersetzen
  ok(recipe_save_dir(arg_dir(), &r, NULL, 0));
}

static void h_recipe_del() {
  ok(recipe_delete_dir(arg_dir(), s_server.arg("file").c_str()));
}

// ------------------------------------------------------------
//  Töpfe und Leerspulen
// ------------------------------------------------------------
static void h_pots() {
  pots_load();
  spools_load();
  String s = "{\"pots\":[";
  for (int i = 0; i < g_pot_count; i++) {
    if (i) s += ",";
    s += "{\"name\":\"" + esc(g_pots[i].name) + "\",\"grams\":" + String(g_pots[i].grams, 1) + ",\"color\":" +
         g_pots[i].color + "}";
  }
  s += "],\"spools\":[";
  for (int i = 0; i < g_spool_count; i++) {
    if (i) s += ",";
    s += "{\"name\":\"" + esc(g_spools[i].name) + "\",\"grams\":" + String(g_spools[i].grams, 1) + "}";
  }
  send_json(s + "]}");
}

static void h_pots_save() {
  String body = s_server.arg("plain");
  String item;
  static pot_t tmp[POT_MAX];
  int n = 0, pos = 0;
  while (n < POT_MAX && (pos = jnext(body, "pots", pos, item)) >= 0) {
    memset(&tmp[n], 0, sizeof(pot_t));
    strncpy(tmp[n].name, jval(item, "name").c_str(), sizeof(tmp[0].name) - 1);
    tmp[n].grams = jval(item, "grams").toFloat();
    tmp[n].color = (uint8_t)(jval(item, "color").toInt() % POT_COLORS);
    // Anlegedatum vom gleichnamigen Topf übernehmen
    for (int k = 0; k < g_pot_count; k++)
      if (fabsf(g_pots[k].grams - tmp[n].grams) < 0.05f) strcpy(tmp[n].created, g_pots[k].created);
    if (tmp[n].name[0] && tmp[n].grams > 0) n++;
  }
  for (int i = 0; i < n; i++) g_pots[i] = tmp[i];
  g_pot_count = n;
  pots_save();
  ui_pots_changed();

  int m = 0;
  pos = 0;
  while (m < SPOOL_MAX && (pos = jnext(body, "spools", pos, item)) >= 0) {
    memset(&g_spools[m], 0, sizeof(spool_t));
    strncpy(g_spools[m].name, jval(item, "name").c_str(), sizeof(g_spools[0].name) - 1);
    g_spools[m].grams = jval(item, "grams").toFloat();
    if (g_spools[m].name[0] && g_spools[m].grams > 0) m++;
  }
  g_spool_count = m;
  spools_save();
  ok();
}

// Neuer Topf bzw. neue Leerspule mit dem aktuell aufgelegten Gewicht
static bool weight_ready(float *g) {
  *g = scale_net();
  return scale_stable() && *g >= 5.0f;
}

static void h_pot_add() {
  String body = s_server.arg("plain");
  float g;
  if (!weight_ready(&g)) { ok(false, "Gewicht noch nicht stabil"); return; }
  pots_load();
  if (g_pot_count >= POT_MAX) { ok(false, "Liste voll"); return; }
  pot_t *p = &g_pots[g_pot_count];
  memset(p, 0, sizeof(*p));
  strncpy(p->name, jval(body, "name").c_str(), sizeof(p->name) - 1);
  p->grams = g;
  p->color = (uint8_t)(jval(body, "color").toInt() % POT_COLORS);
  log_today(p->created);
  g_pot_count++;
  pots_save();
  ui_pots_changed();
  sound_play(SND_SAVE);
  ok();
}

static void h_spool_add() {
  String body = s_server.arg("plain");
  float g;
  if (!weight_ready(&g)) { ok(false, "Gewicht noch nicht stabil"); return; }
  spools_load();
  if (g_spool_count >= SPOOL_MAX) { ok(false, "Liste voll"); return; }
  spool_t *p = &g_spools[g_spool_count++];
  memset(p, 0, sizeof(*p));
  strncpy(p->name, jval(body, "name").c_str(), sizeof(p->name) - 1);
  p->grams = g;
  spools_save();
  sound_play(SND_SAVE);
  ok();
}

// ------------------------------------------------------------
//  Protokoll
// ------------------------------------------------------------
static void h_log_days() {
  static char days[30][11];
  static log_entry_t dummy[1];
  int n = log_days(days, 30);
  String s = "[";
  for (int i = 0; i < n; i++) {
    int total = 0;
    log_read(days[i], dummy, 1, &total);
    if (i) s += ",";
    s += "{\"day\":\"" + String(days[i]) + "\",\"count\":" + total + "}";
  }
  send_json(s + "]");
}

static void h_log() {
  static log_entry_t rows[60];
  int total = 0;
  int n = log_read(s_server.arg("day").c_str(), rows, 60, &total);
  String s = "[";
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    char v[16];
    unit_fmt(v, sizeof(v), rows[i].value, unit_index(rows[i].unit));
    s += "{\"time\":\"" + String(rows[i].time) + "\",\"value\":\"" + v + "\",\"unit\":\"" + String(rows[i].unit) +
         "\",\"note\":\"" + esc(rows[i].note) + "\"}";
  }
  send_json(s + "]");
}

static void h_log_file() {
  hit();
  char path[64];
  snprintf(path, sizeof(path), DIR_LOG "/%s.txt", s_server.arg("day").c_str());
  storage_lock();
  File f = storage_ok() ? SD_MMC.open(path, FILE_READ) : File();
  if (!f) {
    storage_unlock();
    s_server.send(404, "text/plain", "nicht gefunden");
    return;
  }
  String name = "attachment; filename=" + s_server.arg("day") + ".txt";
  s_server.sendHeader("Content-Disposition", name);
  s_server.streamFile(f, "text/plain");
  f.close();
  storage_unlock();
}

// Alle Tage in einer CSV-Datei: Datum;Uhrzeit;Wert;Einheit;Notiz
static void h_log_csv() {
  hit();
  static char days[60][11];
  int n = log_days(days, 60);
  s_server.sendHeader("Content-Disposition", "attachment; filename=waage_protokoll.csv");
  s_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  s_server.send(200, "text/csv", "Datum;Uhrzeit;Wert;Einheit;Notiz\n");
  for (int i = n - 1; i >= 0; i--) {  // älteste zuerst
    char path[64];
    snprintf(path, sizeof(path), DIR_LOG "/%s.txt", days[i]);
    storage_lock();
    File f = storage_ok() ? SD_MMC.open(path, FILE_READ) : File();
    if (!f) {
      storage_unlock();
      continue;
    }
    String chunk;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') continue;
      chunk += String(days[i]) + ";" + line + "\n";
      if (chunk.length() > 1024) {
        s_server.sendContent(chunk);
        chunk = "";
      }
    }
    f.close();
    storage_unlock();
    if (chunk.length()) s_server.sendContent(chunk);
  }
  s_server.sendContent("");
}


// ------------------------------------------------------------
//  Dateien auf der SD-Karte: auflisten, herunterladen, hochladen,
//  löschen. Damit kommt man an Rezepte, Sprachdateien und Protokolle,
//  ohne die Karte auszubauen.
//  Aus Sicherheitsgründen bleibt alles unterhalb von /Waage.
// ------------------------------------------------------------
static bool path_ok(const String &p) {
  return p.startsWith("/Waage") && p.indexOf("..") < 0;
}

static void h_files() {
  String dir = s_server.hasArg("dir") ? s_server.arg("dir") : String(DIR_ROOT);
  if (!path_ok(dir)) {
    send_json("{\"err\":\"Pfad nicht erlaubt\"}");
    return;
  }
  storage_lock();
  File d = storage_ok() ? SD_MMC.open(dir) : File();
  String dirs = "", files = "";
  if (d && d.isDirectory()) {
    File f = d.openNextFile();
    while (f) {
      String name = f.name();
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      if (f.isDirectory()) {
        if (dirs.length()) dirs += ",";
        dirs += "\"" + esc(name.c_str()) + "\"";
      } else {
        if (files.length()) files += ",";
        files += "{\"name\":\"" + esc(name.c_str()) + "\",\"size\":" + String((uint32_t)f.size()) + "}";
      }
      f.close();
      f = d.openNextFile();
    }
    d.close();
  }
  uint64_t total = 0, used = 0;
  if (storage_ok()) {
    total = SD_MMC.totalBytes();
    used = SD_MMC.usedBytes();
  }
  storage_unlock();
  String s = "{\"dir\":\"" + esc(dir.c_str()) + "\",\"dirs\":[" + dirs + "],\"files\":[" + files +
             "],\"total\":" + String((uint32_t)(total / 1024)) + ",\"used\":" + String((uint32_t)(used / 1024)) + "}";
  send_json(s);
}

static void h_file_get() {
  hit();
  String path = s_server.arg("path");
  if (!path_ok(path)) {
    s_server.send(403, "text/plain", "Pfad nicht erlaubt");
    return;
  }
  storage_lock();
  File f = storage_ok() ? SD_MMC.open(path, FILE_READ) : File();
  if (!f || f.isDirectory()) {
    storage_unlock();
    s_server.send(404, "text/plain", "nicht gefunden");
    return;
  }
  String name = path.substring(path.lastIndexOf('/') + 1);
  s_server.sendHeader("Content-Disposition", "attachment; filename=" + name);
  const char *type = name.endsWith(".wav") ? "audio/wav" : (name.endsWith(".csv") ? "text/csv" : "text/plain");
  s_server.streamFile(f, type);
  f.close();
  storage_unlock();
}

static void h_file_del() {
  String path = s_server.arg("path");
  if (!path_ok(path)) {
    ok(false, "Pfad nicht erlaubt");
    return;
  }
  ok(storage_remove(path.c_str()));
}

static void h_mkdir() {
  String path = s_server.arg("path");
  if (!path_ok(path)) {
    ok(false, "Pfad nicht erlaubt");
    return;
  }
  ok(storage_mkdir(path.c_str()));
}

// Hochladen: die Datei kommt stückweise, deshalb offen halten
static File s_up;
static bool s_up_ok = false;

static void h_upload_done() {
  hit();
  ok(s_up_ok);
}

static void h_upload() {
  HTTPUpload &up = s_server.upload();
  if (up.status == UPLOAD_FILE_START) {
    String dir = s_server.hasArg("dir") ? s_server.arg("dir") : String(DIR_ROOT);
    if (!path_ok(dir) || !storage_ok()) {
      s_up_ok = false;
      return;
    }
    String path = dir + "/" + up.filename;
    storage_lock();
    SD_MMC.remove(path);
    s_up = SD_MMC.open(path, FILE_WRITE);
    storage_unlock();
    s_up_ok = (bool)s_up;
    printf("Upload: %s\r\n", path.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE && s_up) {
    storage_lock();
    if (s_up.write(up.buf, up.currentSize) != up.currentSize) s_up_ok = false;
    storage_unlock();
    hit();
  } else if (up.status == UPLOAD_FILE_END) {
    if (s_up) {
      storage_lock();
      s_up.close();
      storage_unlock();
    }
    sound_play(s_up_ok ? SND_SAVE : SND_WARN);
  }
}

// ------------------------------------------------------------
//  Einstellungen
// ------------------------------------------------------------
static void h_battery() {
  bat_state_t st = hal_battery_state();
  const char *sn = st == BAT_CHARGING ? "laedt" : (st == BAT_FULL ? "voll" : (st == BAT_NONE ? "kein" : "akku"));
  String s = "{\"pct\":" + String(hal_battery_percent()) + ",\"volts\":" + String(hal_battery_volts(), 2) +
             ",\"state\":\"" + sn + "\",\"per\":" + String(batt_per_hour(), 1) + ",\"left\":" +
             String(batt_hours_left(), 1) + ",\"interval\":" + String(BATT_INTERVAL_MS / 60000) +
             ",\"test\":" + (batt_test_running() ? "true" : "false") + ",\"test_h\":" +
             String(batt_test_hours(), 2) + ",\"test_v0\":" + String(batt_test_v_start(), 2) +
             ",\"test_n\":" + String(batt_test_samples()) + ",\"history\":[";
  int n = batt_count();
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    s += batt_percent_at(i);
  }
  send_json(s + "]}");
}

static void h_battery_test() {
  if (s_server.arg("on") == "1") batt_test_start();
  else batt_test_stop();
  ok();
}

static void h_update_mode() {
  if (s_server.arg("on") == "1") web_update_mode(15);
  else web_update_mode(0);
  ok(web_update_mode_active());
}

// ---- Bestenliste: Ergebnisse aller Spiele in einer Datei ----
static void h_scores() {
  static score_t rows[60];
  int n = scores_load(rows, 60);
  String s = "[";
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    s += "{\"day\":\"" + esc(rows[i].day) + "\",\"game\":\"" + esc(rows[i].game) + "\",\"name\":\"" +
         esc(rows[i].name) + "\",\"value\":" + String(rows[i].value, 1) + ",\"unit\":\"" + esc(rows[i].unit) + "\"}";
  }
  send_json(s + "]");
}

static void h_score_add() {
  String b = s_server.arg("plain");
  String game = jval(b, "game"), name = jval(b, "name"), unit = jval(b, "unit");
  float v = jval(b, "value").toFloat();
  if (!game.length() || !name.length()) {
    ok(false, "Spiel und Name nötig");
    return;
  }
  player_add(name.c_str());  // neue Namen wandern in die Spielerliste
  ok(score_add(game.c_str(), name.c_str(), v, unit.length() ? unit.c_str() : "g"));
}

static void h_player_add() {
  String name = jval(s_server.arg("plain"), "name");
  if (!name.length()) name = s_server.arg("name");
  ok(name.length() ? player_add(name.c_str()) : false);
}

static void h_settings() {
  porto_load();
  int h = 0, m = 0;
  hal_time(&h, &m);
  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", h, m);
  String s = "{\"unit\":" + String(g_set.unit) + ",\"autosave\":" + (g_set.autosave ? "true" : "false") + ",\"autonext\":" + (g_set.auto_next ? "true" : "false") +
             ",\"idle\":" + g_set.idle_min + ",\"autooff\":" + g_set.auto_off_min + ",\"autotara\":" + (g_set.autotara ? "true" : "false") +
             ",\"cd\":" + g_set.countdown_s + ",\"tol\":" + g_set.tol_g + ",\"vol\":" + g_set.volume +
             ",\"scheme\":" + g_set.scheme + ",\"speak\":" + (g_set.speak ? "true" : "false") +
             ",\"speak_ev\":" + (g_set.speak_ev ? "true" : "false") +
             ",\"voice_on\":" + (g_set.voice_on ? "true" : "false") + ",\"voice_ok\":" + (voice_available() ? "true" : "false") +
             ",\"voice\":\"" + esc(g_set.voice) + "\",\"time\":\"" + t + "\",\"ssid\":\"" + esc(g_set.ssid) +
             "\",\"factor\":" + String(scale_factor(), 3) + ",\"lang\":" + g_set.lang + ",\"precise\":" + (g_set.precise ? "true" : "false") + ",\"azt\":" + (g_set.azt ? "true" : "false") +
             ",\"liquid\":" + g_set.liquid;
  {  // Prüfgewicht
    char last[16], next[16];
    refchk_date(g_set.ref_last, last, sizeof(last));
    int dl = refchk_days_left();
    if (g_set.ref_last > 0 && dl < 9999) refchk_date(g_set.ref_last + g_set.ref_days, next, sizeof(next));
    else next[0] = 0;
    s += ",\"ref_g\":" + String(g_set.ref_g, 2) + ",\"ref_days\":" + g_set.ref_days + ",\"ref_tol\":" +
         String(g_set.ref_tol, 2) + ",\"ref_last\":\"" + (g_set.ref_last > 0 ? last : "") + "\",\"ref_meas\":" +
         String(g_set.ref_meas, 2) + ",\"ref_ok\":" + (g_set.ref_ok ? "true" : "false") + ",\"ref_next\":\"" + next +
         "\",\"ref_due\":" + (refchk_due() && g_set.ref_g > 0 ? "true" : "false");
  }
  s += ",\"liquids\":[";
  for (int i = 0; i < liquid_count(); i++) {
    if (i) s += ",";
    s += "\"" + esc(liquid_name(i)) + "\"";
  }
  s += "],\"voices\":[";
  static char packs[VOICE_PACKS_MAX][24];
  int n = sound_voice_packs(packs, VOICE_PACKS_MAX);
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    s += "\"" + esc(packs[i]) + "\"";
  }
  s += "],\"porto\":[";
  for (int i = 0; i < g_porto_count; i++) {
    if (i) s += ",";
    s += "{\"name\":\"" + esc(g_porto[i].name) + "\",\"max\":" + g_porto[i].max_g + ",\"ct\":" + g_porto[i].price_ct + "}";
  }
  send_json(s + "]}");
}

static void h_settings_save() {
  String b = s_server.arg("plain");
  g_set.unit = constrain((int)jval(b, "unit").toInt(), 0, UNIT_COUNT - 1);
  g_set.autosave = jval(b, "autosave") == "true";
  if (jval(b, "autonext").length()) g_set.auto_next = jval(b, "autonext") == "true";
  g_set.idle_min = constrain((int)jval(b, "idle").toInt(), 0, 60);
  if (jval(b, "liquid").length()) g_set.liquid = constrain((int)jval(b, "liquid").toInt(), 0, liquid_count() - 1);
  if (jval(b, "precise").length()) g_set.precise = jval(b, "precise") == "true";
  if (jval(b, "ref_g").length()) g_set.ref_g = constrain(jval(b, "ref_g").toFloat(), 0.0f, 3000.0f);
  if (jval(b, "ref_days").length()) g_set.ref_days = constrain((int)jval(b, "ref_days").toInt(), 0, 365);
  if (jval(b, "ref_tol").length()) g_set.ref_tol = constrain(jval(b, "ref_tol").toFloat(), 0.05f, 50.0f);
  if (jval(b, "azt").length()) {
    g_set.azt = jval(b, "azt") == "true";
    scale_set_autozero(g_set.azt);
  }
  if (jval(b, "autooff").length()) g_set.auto_off_min = constrain((int)jval(b, "autooff").toInt(), 0, 120);
  g_set.autotara = jval(b, "autotara") == "true";
  g_set.countdown_s = constrain((int)jval(b, "cd").toInt(), 1, 10);
  g_set.tol_g = constrain((int)jval(b, "tol").toInt(), 1, 50);
  g_set.volume = constrain((int)jval(b, "vol").toInt(), 0, 100);
  g_set.scheme = constrain((int)jval(b, "scheme").toInt(), 0, SCHEME_COUNT - 1);
  g_set.speak = jval(b, "speak") == "true";
  if (jval(b, "speak_ev").length()) g_set.speak_ev = jval(b, "speak_ev") == "true";
  // Sprachbefehle (nur wenn ESP-SR einkompiliert ist)
  if (jval(b, "voice_on").length() && voice_available() && !voice_starting()) {
    bool on = jval(b, "voice_on") == "true";
    if (on != g_set.voice_on) {
      g_set.voice_on = on;
      if (on) voice_begin();
      else voice_stop();
    }
  }
  String voice = jval(b, "voice");
  if (voice.length()) sound_voice_set(voice.c_str());  // prüft das neue Paket gleich
  settings_save();

  String item;
  int n = 0, pos = 0;
  static porto_t tmp[PORTO_MAX];
  while (n < PORTO_MAX && (pos = jnext(b, "porto", pos, item)) >= 0) {
    memset(&tmp[n], 0, sizeof(porto_t));
    strncpy(tmp[n].name, jval(item, "name").c_str(), sizeof(tmp[0].name) - 1);
    tmp[n].max_g = jval(item, "max").toInt();
    tmp[n].price_ct = jval(item, "ct").toInt();
    if (tmp[n].name[0] && tmp[n].max_g > 0) n++;
  }
  if (n > 0) {
    for (int i = 0; i < n; i++) g_porto[i] = tmp[i];
    g_porto_count = n;
    porto_save();
  }
  ok();
}

// Gespeicherte WLAN-Netze (Passwörter werden nie ausgegeben)
static void h_wlan() {
  String s = "{\"cur\":\"" + esc(g_set.ssid) + "\",\"max\":" + NET_MAX + ",\"nets\":[";
  for (int i = 0; i < net_count(); i++) {
    if (i) s += ",";
    s += "\"" + esc(net_ssid(i)) + "\"";
  }
  send_json(s + "]}");
}

static void h_wlan_add() {
  String b = s_server.arg("plain");
  String ssid = junesc(jval(b, "ssid")), pass = junesc(jval(b, "pass"));
  if (!ssid.length() || ssid.length() > 32 || pass.length() > 64) {
    ok(false, "Name oder Passwort ungültig");
    return;
  }
  net_store(ssid.c_str(), pass.c_str());  // bestehende Verbindung bleibt, sonst bricht die Seite ab
  ok();
}

static void h_wlan_del() {
  if (!s_server.hasArg("i")) {
    ok(false, "Fehler");
    return;
  }
  net_forget(s_server.arg("i").toInt());
  ok();
}

// ------------------------------------------------------------
//  Küchentimer und Langzeitmessung (laufen auf der Waage weiter)
// ------------------------------------------------------------
static void h_tools() {
  String s = "{\"timers\":[";
  for (int i = 0; i < TIMER_COUNT; i++) {
    if (i) s += ",";
    s += "{\"run\":" + String(timer_running(i) ? "true" : "false") + ",\"ring\":" +
         (timer_ringing(i) ? "true" : "false") + ",\"left\":" + timer_left(i) + ",\"dur\":" + timer_duration(i) +
         ",\"name\":\"" + esc(timer_name(i)) + "\"}";
  }
  float h[LT_HIST];
  int n = lt_hist(h, 60);
  s += "],\"lt\":{\"run\":" + String(lt_running() ? "true" : "false") + ",\"iv\":" + lt_interval() +
       ",\"n\":" + lt_samples() + ",\"h\":" + String(lt_hours(), 3) + ",\"trend\":" + String(lt_trend(), 2) +
       ",\"first\":" + String(lt_first(), 2) + ",\"file\":\"" + esc(lt_file()) + "\",\"hist\":[";
  for (int i = 0; i < n; i++) s += (i ? "," : "") + String(h[i], 2);
  send_json(s + "]}}");
}

// ?ringoff=1 Alarm aus, ?i=0&s=300&name=Nudeln starten, ?i=0&stop=1 stoppen/Alarm aus, ?i=0&add=60 verlängern
static void h_timer() {
  if (s_server.hasArg("ringoff")) {  // alle klingelnden Timer aus (Hinweis auf der Startseite)
    for (int k = 0; k < TIMER_COUNT; k++)
      if (timer_ringing(k)) timer_stop(k);
    ok();
    return;
  }
  int i = s_server.arg("i").toInt();
  if (i < 0 || i >= TIMER_COUNT) {
    ok(false, "Fehler");
    return;
  }
  if (s_server.hasArg("stop")) timer_stop(i);
  else if (s_server.hasArg("add")) timer_add(i, s_server.arg("add").toInt());
  else {
    int secs = s_server.arg("s").toInt();
    if (secs < 1 || secs > 24 * 3600) {
      ok(false, "Fehler");
      return;
    }
    String name = s_server.arg("name");
    name.trim();
    if (name.length() > 19) name = name.substring(0, 19);
    timer_start(i, secs, name.c_str());
    sound_play(SND_CLICK);
  }
  ok();
}

// ?on=1&iv=60 starten (braucht SD-Karte), ?on=0 beenden
static void h_lt() {
  if (s_server.arg("on") == "1") {
    if (lt_running()) {
      ok();
      return;
    }
    int iv = constrain((int)s_server.arg("iv").toInt(), 1, 3600);
    ok(lt_start(iv), "Keine SD-Karte");
  } else {
    lt_stop();
    ok();
  }
}

// Online-Update von GitHub: ?check=1 suchen, ?install=1 installieren, sonst nur Zustand
static void h_ota() {
  if (s_server.hasArg("check")) upd_check();
  else if (s_server.hasArg("install")) upd_install();
  static const char *const ST[] = { "idle", "connect", "check", "latest", "available", "download", "done", "error" };
  String s = "{\"ver\":\"" + esc(fw_version()) + "\",\"local\":" + (fw_is_local() ? "true" : "false") +
             ",\"repo\":\"" + esc(upd_repo()) + "\",\"state\":\"" + ST[upd_state()] + "\",\"latest\":\"" +
             esc(upd_latest()) + "\",\"progress\":" + upd_progress() + ",\"error\":\"" + esc(upd_error()) + "\"}";
  send_json(s);
}

// Sprache umstellen (0 Deutsch, 1 English): gilt für Anzeige, Weboberfläche und Ansage
static void h_lang() {
  int l = constrain((int)s_server.arg("l").toInt(), 0, 1);
  if (l != g_set.lang) {
    g_set.lang = l;
    ui_lang_changed();  // Startseite neu aufbauen, Stimmpaket wählen, speichern
  }
  ok();
}

static void h_sync() {
  net_sync_now();
  ok();
}

static void h_level_reset() {
  g_set.lvl_off_x = 0;
  g_set.lvl_off_y = 0;
  settings_save();
  ok();
}

static void h_speak_test() {
  bool was = g_set.speak;
  g_set.speak = true;  // Hörprobe auch bei ausgeschalteter Ansage
  sound_speak_weight(1234.5f, g_set.unit);
  g_set.speak = was;
  ok();
}

// ------------------------------------------------------------
//  Firmware aktualisieren (OTA)
//  Zwei Wege: über den Browser (/update) und über die Arduino-IDE,
//  die die Waage dann als Netzwerk-Port "waage" anbietet.
// ------------------------------------------------------------
static const char UPDATE_PAGE[] PROGMEM =
    "<!doctype html><html lang=de><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'><title>Firmware</title>"
    "<style>body{font:16px system-ui;background:#F6F4EE;color:#1C1C19;margin:0;padding:24px}"
    ".c{max-width:480px;margin:auto;background:#fff;border:1px solid #E2DED3;border-radius:18px;padding:20px}"
    "h1{font-size:22px;margin:0 0 12px}input,button{font:16px system-ui;margin-top:12px}"
    "button{min-height:46px;padding:0 18px;border-radius:12px;border:0;background:#17784F;color:#fff}"
    "p{color:#5E5B52}</style></head><body><div class=c><h1>Firmware aktualisieren</h1>"
    "<p>Datei <b>Waage.ino.bin</b> aus dem Arduino-Ordner wählen (Sketch &rarr; Kompilierte Binärdatei exportieren). "
    "Die Waage startet danach neu.</p>"
    "<form method=POST action='/update' enctype='multipart/form-data'>"
    "<input type=file name=update accept='.bin' required><br><button type=submit>Hochladen</button></form>"
    "</div></body></html>";

static void h_update_page() {
  hit();
  s_server.send_P(200, "text/html; charset=utf-8", UPDATE_PAGE);
}

static void h_update_done() {
  hit();
  s_server.send(200, "text/html; charset=utf-8",
                s_update_ok ? "<h2>Fertig. Die Waage startet neu.</h2>"
                            : "<h2>Fehlgeschlagen. Bitte erneut versuchen.</h2>");
  if (s_update_ok) {
    delay(500);
    ESP.restart();
  }
}

static void h_update_upload() {
  HTTPUpload &up = s_server.upload();
  if (up.status == UPLOAD_FILE_START) {
    printf("OTA: %s wird übertragen\r\n", up.filename.c_str());
    s_update_ok = false;
    sound_play(SND_TICK);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    hit();
  } else if (up.status == UPLOAD_FILE_END) {
    s_update_ok = Update.end(true);
    printf("OTA: %s\r\n", s_update_ok ? "erfolgreich" : "fehlgeschlagen");
    sound_play(s_update_ok ? SND_DONE : SND_WARN);
  }
}

static void ota_begin() {
  if (s_ota) return;
  ArduinoOTA.setHostname("waage");
  ArduinoOTA.onStart([]() {
    hit();  // Abschaltzeit zurücksetzen, sonst geht das WLAN mitten im Update aus
    sound_play(SND_TICK);
    printf("OTA: Update über die IDE startet\r\n");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) { hit(); });
  ArduinoOTA.onEnd([]() {
    sound_play(SND_DONE);
    printf("OTA: fertig\r\n");
  });
  ArduinoOTA.onError([](ota_error_t e) {
    sound_play(SND_WARN);
    printf("OTA: Fehler %u\r\n", (unsigned)e);
  });
  ArduinoOTA.begin();
  s_ota = true;
}

// ------------------------------------------------------------
//  Starten und Stoppen
// ------------------------------------------------------------
// ------------------------------------------------------------
//  Captive Portal: Im eigenen WLAN fragen Handys und PCs nach
//  bestimmten Adressen, um zu prüfen, ob es Internet gibt. Wir leiten
//  alles auf die Waage um -> das Handy öffnet die Seite von selbst.
// ------------------------------------------------------------
static void h_captive() {
  hit();
  // Seite schon offen: Prüfanfragen mit "Internet ok" beantworten, sonst
  // trennt sich das Handy vom WLAN der Waage oder schaltet auf mobile Daten
  if (s_portal_ok) {
    String u = s_server.uri();
    if (u.endsWith("204")) s_server.send(204, "text/plain", "");
    else if (u.indexOf("ncsi") >= 0) s_server.send(200, "text/plain", "Microsoft NCSI");
    else if (u.indexOf("connecttest") >= 0) s_server.send(200, "text/plain", "Microsoft Connect Test");
    else s_server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    return;
  }
  String url = String("http://") + WiFi.softAPIP().toString() + "/";
  s_server.sendHeader("Location", url, true);
  s_server.send(302, "text/plain", "");
}

static void h_not_found() {
  // Im eigenen WLAN: fremde Hostnamen auf die Waage umleiten
  if (s_ap && s_server.hostHeader() != WiFi.softAPIP().toString()) {
    h_captive();
    return;
  }
  h_root();
}

static void routes() {
  if (s_routes) return;
  s_routes = true;
  s_server.on("/", h_root);
  // Prüf-Adressen von Android, Apple und Windows
  const char *probes[] = { "/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
                           "/ncsi.txt", "/connecttest.txt", "/redirect", "/fwlink", "/canonical.html",
                           "/success.txt" };
  for (const char *p : probes) s_server.on(p, h_captive);
  s_server.on("/api/state", h_state);
  s_server.on("/api/tara", h_tara);
  s_server.on("/api/save", h_save);
  s_server.on("/api/log_add", h_log_add);
  s_server.on("/api/park", h_park);
  s_server.on("/api/sound", h_sound);
  s_server.on("/api/hide", h_hide);
  s_server.on("/api/ble_send", h_ble_send);
  s_server.on("/api/game", HTTP_POST, h_game);
  s_server.on("/api/players", HTTP_GET, h_players);
  s_server.on("/api/players", HTTP_POST, h_players_save);
  s_server.on("/api/player_add", h_player_add);
  s_server.on("/api/scores", HTTP_GET, h_scores);
  s_server.on("/api/scores", HTTP_POST, h_score_add);
  s_server.on("/api/recipes", h_recipes);
  s_server.on("/api/recipe", HTTP_GET, h_recipe);
  s_server.on("/api/recipe", HTTP_POST, h_recipe_save);
  s_server.on("/api/recipe/del", h_recipe_del);
  s_server.on("/api/pots", HTTP_GET, h_pots);
  s_server.on("/api/pots", HTTP_POST, h_pots_save);
  s_server.on("/api/pot_add", HTTP_POST, h_pot_add);
  s_server.on("/api/spool_add", HTTP_POST, h_spool_add);
  s_server.on("/api/log/days", h_log_days);
  s_server.on("/api/log", h_log);
  s_server.on("/log.txt", h_log_file);
  s_server.on("/log.csv", h_log_csv);
  s_server.on("/api/files", h_files);
  s_server.on("/file", h_file_get);
  s_server.on("/api/file/del", h_file_del);
  s_server.on("/api/file/mkdir", h_mkdir);
  s_server.on("/upload", HTTP_POST, h_upload_done, h_upload);
  s_server.on("/api/battery", h_battery);
  s_server.on("/api/battery/test", h_battery_test);
  s_server.on("/api/update_mode", h_update_mode);
  s_server.on("/api/settings", HTTP_GET, h_settings);
  s_server.on("/api/settings", HTTP_POST, h_settings_save);
  s_server.on("/api/sync", h_sync);
  s_server.on("/api/level_reset", h_level_reset);
  s_server.on("/api/speak_test", h_speak_test);
  s_server.on("/api/lang", h_lang);
  s_server.on("/api/tools", h_tools);
  s_server.on("/api/timer", h_timer);
  s_server.on("/api/lt", h_lt);
  s_server.on("/api/ota", h_ota);
  s_server.on("/api/wlan", HTTP_GET, h_wlan);
  s_server.on("/api/wlan", HTTP_POST, h_wlan_add);
  s_server.on("/api/wlan/del", h_wlan_del);
  s_server.on("/update", HTTP_GET, h_update_page);
  s_server.on("/update", HTTP_POST, h_update_done, h_update_upload);
  s_server.onNotFound(h_not_found);
}

static void start_ap() {
  WiFi.mode(WIFI_AP);
  IPAddress ip(192, 168, 4, 1);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID);
  WiFi.setSleep(false);  // Energiesparen des Funkmoduls aus: sonst reagiert die Seite träge
  delay(100);  // AP braucht einen Moment, bevor DNS startet
  s_ap = true;
  snprintf(s_url, sizeof(s_url), "http://%s", WiFi.softAPIP().toString().c_str());
  snprintf(s_ip, sizeof(s_ip), "%s", WiFi.softAPIP().toString().c_str());
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", WiFi.softAPIP());  // alle Namen -> Waage
  MDNS.begin("waage");
  routes();
  s_server.begin();
  s_running = true;
  hit();
}

void web_start() {
  if (s_running || s_connecting) return;
  s_start_ms = millis();
  hit();
  s_portal_ok = false;
  if (net_has_credentials()) {
    // Ist das WLAN gerade für die Uhrzeit schon verbunden, nicht neu verbinden
    // Sonst das stärkste gespeicherte Netz in Reichweite suchen (net.cpp)
    if (WiFi.status() != WL_CONNECTED) net_connect_start(false);
    WiFi.setAutoReconnect(true);
    s_connecting = true;
    s_ap = false;
  } else {
    start_ap();
  }
}

void web_stop() {
  if (!s_running && !s_connecting) return;
  s_server.stop();
  if (s_ap) s_dns.stop();
  MDNS.end();
  WiFi.softAPdisconnect(true);
  if (!upd_busy()) {  // ein laufendes Online-Update behält seine Verbindung
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  if (s_ota) {
    ArduinoOTA.end();
    s_ota = false;
  }
  s_running = false;
  s_connecting = false;
  s_url[0] = 0;
  s_ip[0] = 0;
  s_ota_until = 0;
  s_portal_ok = false;
  s_hide = false;
  s_park_goal = 0;
}

void web_update_mode(uint32_t minutes) {
  if (!web_running()) web_start();
  s_ota_until = millis() + minutes * 60000UL;
  hit();
}

bool web_update_mode_active() {
  return s_ota_until && (int32_t)(s_ota_until - millis()) > 0;
}

uint32_t web_update_seconds_left() {
  return web_update_mode_active() ? (s_ota_until - millis()) / 1000 : 0;
}

// Ein Browser öffnet Verbindungen gern auf Vorrat, ohne sofort etwas zu
// senden. Der WebServer wartet darauf bis zu 5 Sekunden (HTTP_MAX_DATA_WAIT)
// und nimmt in dieser Zeit keinen anderen Client an - daher die langen
// Ladezeiten. Deshalb trennen wir solche stummen Verbindungen selbst.
#define IDLE_CLIENT_MS 350

static void drop_idle_client() {
  NetworkClient &c = s_server.client();
  static uint32_t quiet_since = 0;
  if (!c || !c.connected() || c.available()) {
    quiet_since = 0;
    return;
  }
  uint32_t now = millis();
  if (quiet_since == 0) {
    quiet_since = now;
    return;
  }
  if (now - quiet_since > IDLE_CLIENT_MS) {
    c.stop();  // macht den Server sofort wieder frei
    quiet_since = 0;
  }
}

void web_loop() {
  if (s_connecting) {
    if (WiFi.status() == WL_CONNECTED && !net_connect_busy()) {
      s_connecting = false;
      MDNS.begin("waage");
      routes();
      s_server.begin();
      s_running = true;
      snprintf(s_url, sizeof(s_url), "http://waage.local");
      snprintf(s_ip, sizeof(s_ip), "%s", WiFi.localIP().toString().c_str());
      WiFi.setSleep(false);
      ota_begin();  // Netzwerk-Port für die Arduino-IDE  // Energiesparen aus: sonst reagiert die Seite träge
      hit();
    } else if (!net_connect_busy() || millis() - s_start_ms > CONNECT_TIMEOUT_MS + NET_MAX * 10000UL) {
      s_connecting = false;  // kein gespeichertes Netz erreichbar -> eigenes WLAN
      WiFi.disconnect(true);
      start_ap();
    }
    return;
  }
  if (!s_running) return;
  if (s_ap) s_dns.processNextRequest();
  if (web_update_mode_active() || upd_busy()) hit();  // Abschalttimer anhalten (auch beim Online-Update)
  if (s_ota) ArduinoOTA.handle();

  // mehrere Schritte je Aufruf: eine Anfrage braucht Annehmen, Lesen und
  // Antworten, sonst dauert jede Anfrage mehrere Anzeigebilder
  uint32_t t0 = millis();
  do {
    drop_idle_client();
    s_server.handleClient();
  } while (millis() - t0 < 6 && s_server.client() && s_server.client().connected());
  if (s_park_goal > 0) sound_parking(scale_net(), s_park_goal, 2.0f);  // Parkpiepser für den Browser
  if (millis() - s_last_hit > IDLE_OFF_MS) web_stop();
}

bool web_running() {
  return s_running || s_connecting;
}

bool web_ap_mode() {
  return s_ap;
}

const char *web_url() {
  return s_url;
}

const char *web_ip() {
  return s_ip;
}

const char *web_ssid() {
  return AP_SSID;
}

uint32_t web_seconds_left() {
  if (!s_running) return 0;
  uint32_t el = millis() - s_last_hit;
  return el >= IDLE_OFF_MS ? 0 : (IDLE_OFF_MS - el) / 1000;
}

bool web_hide_weight() {
  return s_running && s_hide;
}
