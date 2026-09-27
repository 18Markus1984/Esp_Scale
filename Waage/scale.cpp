#include "scale.h"
#include "config.h"
#include "hal.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <Preferences.h>
#endif

#define HIST_LEN 10          // 10 Werte = 1 s bei 100 ms
#define STABLE_SPAN_G 1.0f   // max. Schwankung, um als stabil zu gelten
#define ZERO_BAND_G 0.5f     // kleine Werte um 0 als 0 anzeigen
#define JUMP_G 1.5f          // ab dieser Änderung sofort folgen (etwas wird aufgelegt)
#define SMOOTH 0.25f         // sonst langsam nachziehen
#define MIN_CAL_COUNTS 500   // Mindestunterschied beim Kalibrieren

static long  s_counts = 0;      // letzter Rohwert
static long  s_zero = 0;        // Rohwert der leeren Waage
static float s_factor = 420.0f; // Zählwerte pro Gramm (erster Abschnitt)
static long  s_cal_raw[CAL_MAX];    // Rohwerte der Referenzpunkte
static float s_cal_g[CAL_MAX];      // zugehörige Sollgewichte
static int   s_cal_n = 0;           // Anzahl Referenzpunkte
static float s_raw = 0.0f;      // Brutto in g (ungefiltert, für Logik)
static float s_shown = 0.0f;    // geglättet, für die Anzeige
static float s_tare = 0.0f;     // Tara in g

// Präzisionsmodus: solange das Gewicht ruhig liegt, bis zu 5 s mitteln
#define PREC_LEN 50
#define PREC_MIN 10                 // ab so vielen Werten (1 s) gilt der Mittelwert
static float s_prec[PREC_LEN];
static int s_prec_n = 0, s_prec_pos = 0;

// Nullpunkt-Nachführung: langsame Drift der leeren Waage wegregeln
#define AZT_BAND_G 0.5f    // nur so nah an 0
#define AZT_ARM_G 0.05f    // "wirklich leer": ab hier wird nachgeführt
#define AZT_STEP_G 0.08f   // Änderung pro Sekunde darüber = etwas aufgelegt, nicht Drift
#define AZT_GAIN 0.05f     // Anteil der Abweichung, der je 100 ms korrigiert wird
static bool s_azt = true;
static bool s_azt_armed = false;
static float s_azt_hist[HIST_LEN];  // Brutto der letzten Sekunde

static float s_hist[HIST_LEN];  // Gramm für die Stabilität
static long  s_chist[HIST_LEN]; // Rohwerte für die Kalibrierung
static int s_hist_count = 0;
static int s_hist_pos = 0;

#if SIM_WAAGE
// ------------------------------------------------------------
//  Simulation: Gewichte werden nacheinander "aufgelegt"
// ------------------------------------------------------------
#define SIM_COUNTS_PER_G 420.0f
struct SimStep {
  float grams;
  uint32_t duration_ms;
};
static const SimStep SIM_STEPS[] = {
  { 0.0f, 5000 },
  { 1234.5f, 7000 },
  { 1840.2f, 6000 },
  { 3120.0f, 4000 },  // Überlast
  { 812.4f, 7000 },
  { 0.0f, 4000 },
};
static const int SIM_COUNT = sizeof(SIM_STEPS) / sizeof(SIM_STEPS[0]);
static int sim_index = 0;
static uint32_t sim_start = 0;
static float sim_value = 0.0f;

static void hw_init() {
  sim_start = hal_millis();
}

float sim_force_g = -1.0f;  // Testumgebung: >= 0 legt dieses Gewicht fest auf

static long read_counts() {
  uint32_t now = hal_millis();
  if (sim_force_g >= 0.0f) {
    sim_value += (sim_force_g - sim_value) * 0.25f;
    float noise = ((rand() % 100) - 50) / 200.0f;
    return (long)((sim_value + noise) * SIM_COUNTS_PER_G);
  }
  if (now - sim_start > SIM_STEPS[sim_index].duration_ms) {
    sim_index = (sim_index + 1) % SIM_COUNT;
    sim_start = now;
  }
  // weich zum Zielwert laufen, wie beim echten Auflegen
  sim_value += (SIM_STEPS[sim_index].grams - sim_value) * 0.25f;
  float noise = ((rand() % 100) - 50) / 200.0f;  // ±0,25 g
  return (long)((sim_value + noise) * SIM_COUNTS_PER_G);
}

#else
// ------------------------------------------------------------
//  Echter HX711 am UART-Header: DOUT -> RXD (GPIO44), SCK -> TXD (GPIO43)
// ------------------------------------------------------------
#include <HX711.h>

#define PIN_HX_DOUT 44
#define PIN_HX_SCK  43

static HX711 s_hx;

static void hw_init() {
  s_hx.begin(PIN_HX_DOUT, PIN_HX_SCK);  // Kanal A, Verstärkung 128, 10 Werte/s
  Preferences p;
  p.begin("waage", true);
  float f = p.getFloat("cal", 420.0f);
  if (f != 0.0f) s_factor = f;
  // Mehrpunkt-Kalibrierung, falls vorhanden
  s_cal_n = p.getInt("cal_n", 0);
  if (s_cal_n > CAL_MAX) s_cal_n = CAL_MAX;
  for (int i = 0; i < s_cal_n; i++) {
    char k[8];
    snprintf(k, sizeof(k), "cal_r%d", i);
    s_cal_raw[i] = p.getLong(k, 0);
    snprintf(k, sizeof(k), "cal_g%d", i);
    s_cal_g[i] = p.getFloat(k, 0);
  }
  long saved_zero = p.getLong("cal_zero", 0);
  p.end();
  (void)saved_zero;
  // Nullpunkt beim Einschalten messen (Waage muss leer sein)
  bool ok = s_hx.wait_ready_timeout(1000);
  if (ok) s_zero = s_hx.read_average(10);  // ca. 1 s
  s_counts = s_zero;
  printf("HX711: %s, Faktor %.3f\r\n", ok ? "gefunden" : "NICHT gefunden", s_factor);
}

static long read_counts() {
  // 10 neue Werte pro Sekunde -> passt zum 100-ms-Takt.
  // Ist gerade kein neuer Wert fertig, bleibt der letzte stehen.
  if (s_hx.is_ready()) s_counts = s_hx.read();
  return s_counts;
}
#endif

void scale_init() {
  hw_init();
}

// Rohwert -> Gramm. Mit mehreren Punkten abschnittsweise linear.
static float counts_to_g(long c) {
  if (s_cal_n < 2) return (c - s_zero) / s_factor;
  long x0 = s_zero;
  float y0 = 0;
  for (int i = 0; i < s_cal_n; i++) {
    long x1 = s_cal_raw[i];
    float y1 = s_cal_g[i];
    bool last = (i == s_cal_n - 1);
    if (c <= x1 || last) {
      float span = (float)(x1 - x0);
      if (span == 0) return y1;
      return y0 + (c - x0) * (y1 - y0) / span;  // innerhalb bzw. verlängert
    }
    x0 = x1;
    y0 = y1;
  }
  return 0;
}

void scale_update() {
  s_counts = read_counts();
  s_raw = counts_to_g(s_counts);

  // Anzeige glätten: große Änderungen sofort übernehmen (es wird etwas
  // aufgelegt), kleine Schwankungen wegmitteln. Liegt das Gewicht ruhig,
  // wird der Mittelwert des Fensters gezeigt, dann steht die Zahl still.
  if (fabsf(s_raw - s_shown) > JUMP_G) s_shown = s_raw;
  else s_shown += (s_raw - s_shown) * SMOOTH;
  s_hist[s_hist_pos] = s_raw;
  s_chist[s_hist_pos] = s_counts;
  s_hist_pos = (s_hist_pos + 1) % HIST_LEN;
  if (s_hist_count < HIST_LEN) s_hist_count++;

  if (scale_stable()) {  // ruhig: Mittelwert des Fensters anzeigen
    float sum = 0;
    for (int i = 0; i < s_hist_count; i++) sum += s_hist[i];
    s_shown = sum / s_hist_count;
    s_prec[s_prec_pos] = s_raw;  // für den Präzisionsmodus sammeln
    s_prec_pos = (s_prec_pos + 1) % PREC_LEN;
    if (s_prec_n < PREC_LEN) s_prec_n++;
  } else {
    s_prec_n = 0;  // bewegt: neu anfangen
    s_prec_pos = 0;
  }

  // Nullpunkt-Nachführung: nur bei ruhiger, (fast) leerer Waage und nur,
  // solange sich das Brutto innerhalb einer Sekunde kaum ändert. Ein
  // aufgelegtes Gramm (Sprung) sperrt sie, bis die Waage wieder leer ist.
  float before = s_azt_hist[s_hist_pos];  // Wert von vor 1 s (gleicher Ringplatz)
  s_azt_hist[s_hist_pos == 0 ? HIST_LEN - 1 : s_hist_pos - 1] = s_shown;
  if (s_azt && s_hist_count >= HIST_LEN) {
    float n = s_shown - s_tare;
    if (!scale_stable() || fabsf(s_shown - before) > AZT_STEP_G) s_azt_armed = false;
    else if (fabsf(n) < AZT_ARM_G) s_azt_armed = true;
    if (s_azt_armed && scale_stable() && fabsf(n) < AZT_BAND_G) s_tare += n * AZT_GAIN;
  }
}

void scale_set_autozero(bool on) {
  s_azt = on;
  s_azt_armed = false;
}

static float prec_mean() {
  float sum = 0;
  for (int i = 0; i < s_prec_n; i++) sum += s_prec[i];
  return sum / s_prec_n;
}

bool scale_precise(float *net, float *u, int *n) {
  if (s_prec_n < PREC_MIN) return false;
  float m = prec_mean();
  float var = 0;
  for (int i = 0; i < s_prec_n; i++) var += (s_prec[i] - m) * (s_prec[i] - m);
  float sd = sqrtf(var / (s_prec_n - 1));
  // Unsicherheit des Mittelwerts (2 s / Wurzel n, ca. 95 %), nicht kleiner als 0,01 g.
  // Fehler der Kalibrierung sind darin nicht enthalten.
  float un = 2.0f * sd / sqrtf((float)s_prec_n);
  if (un < 0.01f) un = 0.01f;
  if (net) *net = m - s_tare;
  if (u) *u = un;
  if (n) *n = s_prec_n;
  return true;
}

float scale_gross() {
  return s_shown;
}

float scale_gross_raw() {
  return s_raw;
}

float scale_net() {
  float n = s_shown - s_tare;
  if (fabsf(n) < ZERO_BAND_G) n = 0.0f;
  return n;
}

bool scale_stable() {
  if (s_hist_count < HIST_LEN) return false;
  float lo = s_hist[0], hi = s_hist[0];
  for (int i = 1; i < HIST_LEN; i++) {
    if (s_hist[i] < lo) lo = s_hist[i];
    if (s_hist[i] > hi) hi = s_hist[i];
  }
  return (hi - lo) < STABLE_SPAN_G;
}

bool scale_overload() {
  return s_raw > WAAGE_MAX_G;
}

void scale_tare() {
  // liegt das Gewicht schon länger ruhig, den Mittelwert nehmen (genauer als ein Einzelwert)
  float t = s_prec_n >= PREC_MIN ? prec_mean() : s_raw;
  s_tare = t;
  s_shown = t;
}

float scale_get_tare() {
  return s_tare;
}

void scale_set_tare(float g) {
  s_tare = g;
}

long scale_raw() {
  if (s_hist_count == 0) return s_counts;
  long long sum = 0;
  for (int i = 0; i < s_hist_count; i++) sum += s_chist[i];
  return (long)(sum / s_hist_count);
}

float scale_factor() {
  return s_factor;
}

bool scale_calibrate(long zero_raw, long load_raw, float ref_g) {
  return scale_calibrate_points(zero_raw, &load_raw, &ref_g, 1);
}

int scale_cal_count() {
  return s_cal_n;
}

float scale_cal_gram(int i) {
  return (i >= 0 && i < s_cal_n) ? s_cal_g[i] : 0.0f;
}

bool scale_calibrate_points(long zero_raw, const long *raws, const float *grams, int n) {
  if (n < 1 || n > CAL_MAX) return false;
  if (labs(raws[0] - zero_raw) < MIN_CAL_COUNTS || grams[0] <= 0.0f) return false;
  s_zero = zero_raw;
  s_cal_n = n;
  for (int i = 0; i < n; i++) {
    s_cal_raw[i] = raws[i];
    s_cal_g[i] = grams[i];
  }
  s_factor = (raws[0] - zero_raw) / grams[0];  // erster Abschnitt, auch als Anzeigewert
  s_tare = 0.0f;
  s_hist_count = 0;  // Stabilität neu aufbauen
#if defined(ARDUINO) && !SIM_WAAGE
  Preferences p;
  p.begin("waage", false);
  p.putFloat("cal", s_factor);
  p.putInt("cal_n", n);
  p.putLong("cal_zero", s_zero);
  for (int i = 0; i < n; i++) {
    char k[8];
    snprintf(k, sizeof(k), "cal_r%d", i);
    p.putLong(k, s_cal_raw[i]);
    snprintf(k, sizeof(k), "cal_g%d", i);
    p.putFloat(k, s_cal_g[i]);
  }
  p.end();
#endif
  return true;
}
