#include "refcheck.h"
#include "settings.h"
#include "hal.h"
#include "storage.h"
#include "data.h"
#include "i18n.h"
#include <stdio.h>
#include <math.h>

#define REF_DIR "/Waage/Pruefung"
#define REF_FILE REF_DIR "/pruefgewicht.csv"

// Tage seit 1.1.2000 (gregorianisch, gültig bis 2099)
static int day_number(int y, int m, int d) {
  static const int CUM[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  int yy = y - 2000;
  int n = yy * 365 + (yy + 3) / 4 + CUM[(m - 1) % 12] + d - 1;
  if (m > 2 && (y % 4) == 0) n++;
  return n;
}

int refchk_today() {
  int y, mo, d, h, mi, s;
  if (!hal_now(&y, &mo, &d, &h, &mi, &s) || y < 2024) return -1;
  return day_number(y, mo, d);
}

int refchk_days_left() {
  if (g_set.ref_g <= 0 || g_set.ref_days <= 0) return 9999;
  if (g_set.ref_last <= 0) return 0;  // noch nie geprüft: gleich fällig
  if (!g_set.ref_ok) return 0;         // letzte Prüfung außer Toleranz: bleibt fällig (z. B. nach dem Kalibrieren)
  int t = refchk_today();
  if (t < 0) return 9999;              // ohne Uhrzeit keine Erinnerung
  return g_set.ref_last + g_set.ref_days - t;
}

bool refchk_due() {
  return refchk_days_left() <= 0;
}

void refchk_date(int day, char *b, int len) {
  if (day <= 0) {
    snprintf(b, len, "-");
    return;
  }
  int y = 2000, rest = day;
  while (true) {
    int dy = (y % 4 == 0) ? 366 : 365;
    if (rest < dy) break;
    rest -= dy;
    y++;
  }
  static const int ML[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  int m = 0;
  while (true) {
    int ml = ML[m] + (m == 1 && y % 4 == 0 ? 1 : 0);
    if (rest < ml) break;
    rest -= ml;
    m++;
  }
  snprintf(b, len, "%02d.%02d.%04d", rest + 1, m + 1, y);
}

bool refchk_store(float measured) {
  float dev = measured - g_set.ref_g;
  bool ok = fabsf(dev) <= g_set.ref_tol;
  int t = refchk_today();
  g_set.ref_last = t > 0 ? t : g_set.ref_last;
  g_set.ref_meas = measured;
  g_set.ref_ok = ok;
  settings_save();
  if (storage_ok()) {
    storage_mkdir(REF_DIR);
    if (!storage_exists(REF_FILE)) storage_write(REF_FILE, "Datum;Soll_g;Ist_g;Abweichung_g;Toleranz_g;OK\n");
    char line[96], d[16], s1[16], s2[16], s3[16], s4[16];
    refchk_date(t, d, sizeof(d));
    fmt_num(s1, sizeof(s1), g_set.ref_g, 2);
    fmt_num(s2, sizeof(s2), measured, 2);
    fmt_num(s3, sizeof(s3), dev, 2);
    fmt_num(s4, sizeof(s4), g_set.ref_tol, 2);
    snprintf(line, sizeof(line), "%s;%s;%s;%s;%s;%s", d, s1, s2, s3, s4, ok ? "ja" : "nein");
    storage_append(REF_FILE, line);
  }
  char note[48], w[16];
  fmt_num(w, sizeof(w), dev, 2);
  snprintf(note, sizeof(note), T("Prüfgewicht: %s g %s"), w, ok ? "OK" : T("außer Toleranz"));
  log_add(measured, note);
  return ok;
}
