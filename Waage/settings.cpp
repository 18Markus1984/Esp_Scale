#include "settings.h"
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

settings_t g_set;

void settings_load() {
  Preferences p;
  p.begin("waage", true);
  g_set.autotara = p.getBool("autotara", true);
  g_set.countdown_s = p.getInt("cd_s", 3);
  g_set.tol_g = p.getInt("tol_g", 5);
  memset(g_set.ssid, 0, sizeof(g_set.ssid));
  memset(g_set.pass, 0, sizeof(g_set.pass));
  p.getString("ssid", g_set.ssid, sizeof(g_set.ssid));
  p.getString("pass", g_set.pass, sizeof(g_set.pass));
  g_set.volume = p.getInt("volume", 60);
  g_set.scheme = p.getInt("scheme", 0);
  g_set.speak = p.getBool("speak", false);
  g_set.voice_on = p.getBool("voice_on", false);
  memset(g_set.voice, 0, sizeof(g_set.voice));
  p.getString("voice", g_set.voice, sizeof(g_set.voice));
  g_set.unit = p.getInt("unit", 0);
  g_set.autosave = p.getBool("autosave", false);
  g_set.auto_next = p.getBool("autonext", true);
  g_set.idle_min = p.getInt("idle_min", 5);
  g_set.auto_off_min = p.getInt("auto_off", 30);
  g_set.lvl_off_x = p.getFloat("lvl_x", 0.0f);
  g_set.lvl_off_y = p.getFloat("lvl_y", 0.0f);
  g_set.precise = p.getBool("precise", false);
  g_set.azt = p.getBool("azt", true);
  g_set.liquid = p.getInt("liquid", 0);
  g_set.lang = p.getInt("lang", 0);
  if (g_set.lang < 0 || g_set.lang > 1) g_set.lang = 0;
  p.end();
}

void settings_save() {
  Preferences p;
  p.begin("waage", false);
  p.putBool("autotara", g_set.autotara);
  p.putInt("cd_s", g_set.countdown_s);
  p.putInt("tol_g", g_set.tol_g);
  p.putString("ssid", g_set.ssid);
  p.putString("pass", g_set.pass);
  p.putInt("volume", g_set.volume);
  p.putInt("scheme", g_set.scheme);
  p.putBool("speak", g_set.speak);
  p.putBool("voice_on", g_set.voice_on);
  p.putString("voice", g_set.voice);
  p.putInt("unit", g_set.unit);
  p.putBool("autosave", g_set.autosave);
  p.putBool("autonext", g_set.auto_next);
  p.putInt("idle_min", g_set.idle_min);
  p.putInt("auto_off", g_set.auto_off_min);
  p.putFloat("lvl_x", g_set.lvl_off_x);
  p.putFloat("lvl_y", g_set.lvl_off_y);
  p.putBool("precise", g_set.precise);
  p.putBool("azt", g_set.azt);
  p.putInt("liquid", g_set.liquid);
  p.putInt("lang", g_set.lang);
  p.end();
}

// ------------------------------------------------------------
//  Uhrzeit sichern: als Text "2026-09-21 14:32" im Flash
// ------------------------------------------------------------
void settings_store_time(int y, int mo, int d, int h, int mi) {
  char b[20];
  snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d", y, mo, d, h, mi);
  Preferences p;
  p.begin("waage", false);
  p.putString("clock", b);
  p.end();
}

bool settings_load_time(int *y, int *mo, int *d, int *h, int *mi) {
  char b[24] = "";
  Preferences p;
  p.begin("waage", true);
  p.getString("clock", b, sizeof(b));
  p.end();
  return sscanf(b, "%d-%d-%d %d:%d", y, mo, d, h, mi) == 5 && *y >= 2024;
}
