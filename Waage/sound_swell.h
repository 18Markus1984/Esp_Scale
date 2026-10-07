#pragma once
// ============================================================
//  Weiche Klänge für Ein- und Ausschalten
//
//  Mehrere Sinus-Teiltöne, jeder mit eigener Hüllkurve: weich
//  ansteigend (Cosinus), kurz halten, dann exponentiell abklingen.
//  Die Töne dürfen sich überlappen, dadurch entsteht ein Akkord, der
//  langsam aufblüht – passend zum Leuchten der Startanimation.
//  Plattformunabhängig, damit die Testumgebung dieselben Samples
//  erzeugen kann wie die Waage.
// ============================================================
#include <math.h>
#include <stdint.h>

#define SWELL_RATE   16000
#define SWELL_PARTS  6
#define SWELL_TAIL_S 0.25f   // letzte 0,25 s sanft auf null

typedef struct {
  float freq;    // Hz
  float start;   // Einsatz in s
  float attack;  // Anstieg in s
  float hold;    // Halten in s
  float decay;   // Zeitkonstante des Abklingens in s
  float gain;
  float detune;  // Hz, leicht verstimmter Zweitton für etwas Schimmer (0 = aus)
} swell_part_t;

typedef struct {
  const swell_part_t *parts;
  uint8_t count;
  float len;   // Gesamtlänge in s
  float norm;  // so skaliert, dass die Spitze genau 1,0 erreicht
} swell_t;

// Einschalten „Aufblühen“: Akkord G4–D5–G5–H5 schwillt gestaffelt an,
// am lautesten mit dem Leuchten; zum Schriftzug ein kleiner heller Funke (G6)
static const swell_part_t SWELL_ON_PARTS[] = {
  {  392.0f, 0.00f, 0.70f, 0.10f, 0.55f, 1.00f, 1.2f },
  {  587.3f, 0.12f, 0.62f, 0.10f, 0.50f, 0.70f, 1.5f },
  {  784.0f, 0.25f, 0.55f, 0.10f, 0.45f, 0.55f, 2.0f },
  {  987.8f, 0.42f, 0.45f, 0.08f, 0.40f, 0.35f, 2.4f },
  { 1568.0f, 0.80f, 0.25f, 0.00f, 0.35f, 0.18f, 0.0f },
};
// Ausschalten „Verglühen“: derselbe Akkord, oben zuerst verklingend,
// der Grundton bleibt am längsten
static const swell_part_t SWELL_OFF_PARTS[] = {
  {  987.8f, 0.00f, 0.08f, 0.00f, 0.16f, 0.35f, 2.4f },
  {  784.0f, 0.00f, 0.10f, 0.02f, 0.22f, 0.55f, 2.0f },
  {  587.3f, 0.00f, 0.12f, 0.05f, 0.30f, 0.70f, 1.5f },
  {  392.0f, 0.00f, 0.15f, 0.08f, 0.40f, 1.00f, 1.2f },
};
// Spitzenwerte der ungeteilten Summe: 2,133 bzw. 1,908 (mit Testumgebung gemessen)
#ifndef SWELL_ON_NORM
#define SWELL_ON_NORM  0.4689f
#define SWELL_OFF_NORM 0.5242f
#endif
static const swell_t SWELL_ON  = { SWELL_ON_PARTS,  5, 2.40f, SWELL_ON_NORM };
static const swell_t SWELL_OFF = { SWELL_OFF_PARTS, 4, 1.20f, SWELL_OFF_NORM };

// Laufzustand (Phasen der Teiltöne)
typedef struct {
  const swell_t *s;
  int k, total;
  float ph[SWELL_PARTS][2];
} swell_state_t;

static inline void swell_start(swell_state_t *st, const swell_t *s) {
  st->s = s;
  st->k = 0;
  st->total = (int)(s->len * SWELL_RATE);
  for (int i = 0; i < SWELL_PARTS; i++) st->ph[i][0] = st->ph[i][1] = 0;
}

static inline float swell_env(const swell_part_t *p, float t) {
  if (t < p->start) return 0.0f;
  t -= p->start;
  if (t < p->attack) return 0.5f - 0.5f * cosf((float)M_PI * t / p->attack);
  t -= p->attack;
  if (t < p->hold) return 1.0f;
  return expf(-(t - p->hold) / p->decay);
}

// Nächste n Samples (Bereich -1..1); Rückgabe: geschriebene Anzahl, 0 = fertig
static inline int swell_render(swell_state_t *st, float *out, int n) {
  const swell_t *s = st->s;
  const float two_pi = 2.0f * (float)M_PI;
  int tail = (int)(SWELL_TAIL_S * SWELL_RATE);
  int i = 0;
  for (; i < n && st->k < st->total; i++, st->k++) {
    float t = (float)st->k / SWELL_RATE;
    float v = 0;
    for (int j = 0; j < s->count; j++) {
      const swell_part_t *p = &s->parts[j];
      float e = swell_env(p, t);
      float w = sinf(st->ph[j][0]);
      if (p->detune > 0) w = 0.8f * w + 0.2f * sinf(st->ph[j][1]);
      v += p->gain * e * w;
      st->ph[j][0] += two_pi * p->freq / SWELL_RATE;
      st->ph[j][1] += two_pi * (p->freq + p->detune) / SWELL_RATE;
      if (st->ph[j][0] > two_pi) st->ph[j][0] -= two_pi;
      if (st->ph[j][1] > two_pi) st->ph[j][1] -= two_pi;
    }
    int left = st->total - st->k;
    if (left < tail) {
      float f = (float)left / tail;
      v *= f * f;
    }
    out[i] = v * s->norm;
  }
  return i;
}
