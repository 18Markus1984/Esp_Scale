// ============================================================
//  Messmittelprüfung: Auswertung und CSV (siehe msa_store.h)
// ============================================================
#include "msa_store.h"
#include "storage.h"
#include "scale.h"
#include "data.h"  // log_today
#include "hal.h"
#include "update_online.h"  // fw_version
#include <math.h>
#include <stdio.h>
#include <string.h>

void msa_calc(const float *v, int n, float nominal, float tol, msa_stats_t *r) {
  memset(r, 0, sizeof(*r));
  r->n = n;
  if (n <= 0) return;
  double sum = 0, lo = v[0], hi = v[0];
  for (int i = 0; i < n; i++) {
    sum += v[i];
    if (v[i] < lo) lo = v[i];
    if (v[i] > hi) hi = v[i];
  }
  r->mean = sum / n;
  double var = 0;
  for (int i = 0; i < n; i++) var += (v[i] - r->mean) * (v[i] - r->mean);
  r->s = n > 1 ? sqrt(var / (n - 1)) : 0;
  r->bias = r->mean - nominal;
  r->cg = r->s > 0 ? 0.2 * tol / (6 * r->s) : 0;
  r->cgk = r->s > 0 ? (0.1 * tol - fabs(r->bias)) / (3 * r->s) : 0;
  r->tmin = 40 * r->s + 10 * fabs(r->bias);
  r->lo = lo;
  r->hi = hi;
  r->ok = r->cg >= 1.33 && r->cgk >= 1.33;
}

// Zahl mit Komma als Dezimalzeichen (Tabellenprogramme mit deutscher Einstellung)
static void num(char *b, int len, double x, int dec) {
  snprintf(b, len, "%.*f", dec, x);
  for (char *c = b; *c; c++)
    if (*c == '.') *c = ',';
}

bool msa_save(float nominal, float tol, const float *v, int n, const char *src, char *name, int name_len) {
  if (name && name_len) name[0] = 0;
  if (!storage_ok() || n <= 0) return false;
  storage_mkdir(MSA_DIR);
  msa_stats_t r;
  msa_calc(v, n, nominal, tol, &r);

  char day[11];
  int h = 0, mi = 0;
  log_today(day);
  bool have_time = day[0] && hal_time(&h, &mi);
  char base[32], path[64];
  if (have_time) snprintf(base, sizeof(base), "%s_%02d%02d", day, h, mi);
  else snprintf(base, sizeof(base), "ohne-Datum_%lu", (unsigned long)(hal_millis() / 1000));
  snprintf(path, sizeof(path), MSA_DIR "/%s.csv", base);
  for (int k = 2; storage_exists(path) && k < 100; k++)  // zwei Prüfungen in derselben Minute
    snprintf(path, sizeof(path), MSA_DIR "/%s_%d.csv", base, k);

  // Kopf: Kennwerte und die Kalibrierung, die bei der Prüfung galt
  static char t[640];  // nicht auf den Stapel
  char a[24], b[24], c[24];
  int L = snprintf(t, sizeof(t), "Messmittelfaehigkeit Verfahren 1\n");
  if (have_time) L += snprintf(t + L, sizeof(t) - L, "Datum;%s %02d:%02d\n", day, h, mi);
  else L += snprintf(t + L, sizeof(t) - L, "Datum;\n");
  L += snprintf(t + L, sizeof(t) - L, "Quelle;%s\nFirmware;%s\n", src ? src : "", fw_version());
  num(a, sizeof(a), nominal, nominal == floorf(nominal) ? 0 : 2);
  num(b, sizeof(b), tol, tol == floorf(tol) ? 0 : 2);
  L += snprintf(t + L, sizeof(t) - L, "Sollwert;%s;g\nToleranz T;%s;g\nn;%d\n", a, b, n);
  num(a, sizeof(a), r.mean, 3);
  num(b, sizeof(b), r.bias, 3);
  num(c, sizeof(c), r.s, 4);
  L += snprintf(t + L, sizeof(t) - L, "Mittelwert;%s;g\nBias;%s;g\ns;%s;g\n", a, b, c);
  num(a, sizeof(a), r.cg, 3);
  num(b, sizeof(b), r.cgk, 3);
  num(c, sizeof(c), r.tmin, 2);
  L += snprintf(t + L, sizeof(t) - L, "Cg;%s\nCgk;%s\nErgebnis;%s\nT_min;%s;g\n", a, b,
                r.ok ? "faehig" : "nicht faehig", c);
  L += snprintf(t + L, sizeof(t) - L, "Kalibriert;%s\nKalibrierpunkte", scale_cal_date());
  for (int i = 0; i < scale_cal_count() && L < (int)sizeof(t) - 16; i++) {
    num(a, sizeof(a), scale_cal_gram(i), 0);
    L += snprintf(t + L, sizeof(t) - L, ";%s", a);
  }
  if (L < (int)sizeof(t) - 24) L += snprintf(t + L, sizeof(t) - L, "\n\nNr;Messwert_g\n");
  if (!storage_write(path, t)) return false;
  // Einzelwerte, zehn je Schreibvorgang
  t[0] = 0;
  L = 0;
  for (int i = 0; i < n; i++) {
    num(a, sizeof(a), v[i], 2);
    L += snprintf(t + L, sizeof(t) - L, "%s%d;%s", L ? "\n" : "", i + 1, a);
    if (i % 10 == 9 || i == n - 1) {
      if (!storage_append(path, t)) return false;
      t[0] = 0;
      L = 0;
    }
  }
  if (name && name_len) snprintf(name, name_len, "%s", strrchr(path, '/') + 1);
  printf("Messmittelpruefung gespeichert: %s\r\n", path);
  return true;
}
