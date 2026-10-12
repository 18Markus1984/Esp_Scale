// ============================================================
//  Akkuspannung glätten (siehe batt_filter.h)
// ============================================================
#include "batt_filter.h"
#include <math.h>
#include <string.h>

#define MED_N 5
#define TAU_FAST_MS 3000.0f
#define TAU_SLOW_MS 30000.0f
#define START_MS 10000UL   // die ersten 10 s schneller einschwingen
#define BACK_PTS 3         // Gegenrichtung erst ab so vielen Prozentpunkten

static float s_med[MED_N];
static int s_nmed = 0, s_imed = 0;
static float s_fast = 0, s_slow = 0;
static uint32_t s_last = 0, s_first = 0;
static bool s_have = false;
static int s_shown = -1;

void bf_reset() {
  s_nmed = s_imed = 0;
  s_fast = s_slow = 0;
  s_last = s_first = 0;
  s_have = false;
  s_shown = -1;
}

static float median() {
  float t[MED_N];
  memcpy(t, s_med, sizeof(float) * s_nmed);
  for (int i = 1; i < s_nmed; i++)  // Einfügesortierung, höchstens 5 Werte
    for (int k = i; k > 0 && t[k] < t[k - 1]; k--) {
      float x = t[k];
      t[k] = t[k - 1];
      t[k - 1] = x;
    }
  return t[s_nmed / 2];
}

void bf_feed(float v, uint32_t now) {
  if (v < 2.5f) {  // kein Akku: neu anfangen, sobald einer da ist
    bf_reset();
    return;
  }
  s_med[s_imed] = v;
  s_imed = (s_imed + 1) % MED_N;
  if (s_nmed < MED_N) s_nmed++;
  float m = median();
  if (!s_have) {
    s_fast = s_slow = m;
    s_last = s_first = now;
    s_have = true;
    return;
  }
  float dt = (float)(now - s_last);
  s_last = now;
  if (dt <= 0) return;
  float tf = TAU_FAST_MS, ts = TAU_SLOW_MS;
  if (now - s_first < START_MS) ts = TAU_FAST_MS;  // gleich nach dem Einschalten nicht ewig warten
  s_fast += (m - s_fast) * (1.0f - expf(-dt / tf));
  s_slow += (m - s_slow) * (1.0f - expf(-dt / ts));
}

bool bf_ready() { return s_have; }
float bf_fast() { return s_fast; }
float bf_slow() { return s_slow; }

int bf_percent(int raw, bool charging) {
  if (raw < 0) return raw;
  if (s_shown < 0) {
    s_shown = raw;
    return raw;
  }
  if (charging) {
    if (raw > s_shown || raw <= s_shown - BACK_PTS) s_shown = raw;
  } else {
    if (raw < s_shown || raw >= s_shown + BACK_PTS) s_shown = raw;
  }
  return s_shown;
}
