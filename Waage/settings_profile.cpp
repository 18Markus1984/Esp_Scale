// ============================================================
//  Profile: Voreinstellungen für Standard, Küche, Werkstatt, Einfach
//  (eigene Datei ohne Flash-Zugriff, damit die Testumgebung sie mitnutzt)
// ============================================================
#include "settings.h"

void settings_profile_apply(int profile) {
  if (profile < 0 || profile >= PROFILE_COUNT) return;
  g_set.profile = profile;
  g_set.modes_off = 0;
  switch (profile) {
    case PROFILE_KUECHE:
      g_set.cats = CAT_KUECHE;
      g_set.autotara = true;
      break;
    case PROFILE_WERKSTATT:
      g_set.cats = CAT_WERKSTATT;
      break;
    case PROFILE_EINFACH:
      // Für schlechte Augen: große Ziffern, Gewicht wird angesagt.
      // Die Farbe bleibt, wie sie ist (passend zum Gehäuse).
      g_set.cats = 0;
      g_set.speak = true;
      g_set.speak_ev = true;
      if (g_set.volume < 80) g_set.volume = 80;
      g_set.autotara = false;   // kein Countdown, der überrascht
      g_set.unit = 0;           // Gramm
      g_set.precise = false;
      g_set.autosave = false;
      if (g_set.auto_off_min && g_set.auto_off_min < 60) g_set.auto_off_min = 60;
      break;
    default:  // Standard
      g_set.cats = CAT_ALL;
      g_set.autosave = true;  // ruhige Wägungen landen von selbst im Protokoll
      break;
  }
}
