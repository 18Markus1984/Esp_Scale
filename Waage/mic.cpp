#include "mic.h"
#include "config.h"
#include <math.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <ESP_I2S.h>

#define MIC_BCK 15
#define MIC_WS  2
#define MIC_DIN 39
#define MIC_RATE 16000
#define CLAP_DB -22.0f     // darüber gilt ein Geräusch als Klatscher
#define CLAP_GAP_MS 250    // Mindestabstand zwischen zwei Klatschern

static I2SClass s_mic;
static bool s_ready = false;
static volatile bool s_listen = false;
static volatile float s_db = -90.0f, s_peak = -90.0f;
static volatile int s_claps = 0;
static volatile int s_channel = 0;   // 0 = links, 1 = rechts
static volatile int s_raw_peak = 0;  // Spitze in Zählwerten (zur Fehlersuche)
static volatile uint32_t s_last_data = 0;
static volatile int s_restarts = 0;
static volatile bool s_exclusive = false;
static volatile bool s_idle = true;

I2SClass &mic_i2s() {
  return s_mic;
}

// Auswertung eines Kanals: Gleichanteil abziehen, dann Effektiv- und Spitzenwert
static void analyse(const int16_t *buf, int frames, int ch, float *rms, int *peak) {
  long sum = 0;
  for (int i = 0; i < frames; i++) sum += buf[i * 2 + ch];
  float dc = (float)sum / frames;  // MEMS-Mikrofone haben immer einen Gleichanteil
  double acc = 0;
  int mx = 0;
  for (int i = 0; i < frames; i++) {
    float v = buf[i * 2 + ch] - dc;
    acc += (double)v * v;
    int a = (int)fabsf(v);
    if (a > mx) mx = a;
  }
  *rms = sqrt(acc / frames);
  *peak = mx;
}

static void mic_task(void *arg) {
  static int16_t buf[512];
  uint32_t last_clap = 0, peak_since = 0, dbg = 0;
  float peak = -90.0f;
  while (true) {
    if (!s_listen || s_exclusive) {  // Spracherkennung liest selbst
      s_idle = true;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    s_idle = false;
    size_t n = s_mic.readBytes((char *)buf, sizeof(buf));
    int frames = n / 4;  // Stereo, 16 Bit
    if (frames < 8) {
      // Kommt länger nichts, hängt der I2S-Empfang: neu starten
      if (millis() - s_last_data > 2000) {
        printf("Mikrofon: keine Daten, starte I2S neu\r\n");
        s_mic.end();
        vTaskDelay(pdMS_TO_TICKS(50));
        s_mic.setPins(MIC_BCK, MIC_WS, -1, MIC_DIN);
        s_mic.begin(I2S_MODE_STD, MIC_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
        s_restarts = s_restarts + 1;
        s_last_data = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    s_last_data = millis();
    // beide Kanäle messen: das Mikrofon liegt je nach Verdrahtung links oder rechts
    float rms_l, rms_r;
    int pk_l, pk_r;
    analyse(buf, frames, 0, &rms_l, &pk_l);
    analyse(buf, frames, 1, &rms_r, &pk_r);
    bool right = rms_r > rms_l;
    s_channel = right ? 1 : 0;
    float rms = right ? rms_r : rms_l;
    int pkv = right ? pk_r : pk_l;
    s_raw_peak = pkv;

    float db = 20.0f * log10f((rms + 1.0f) / 32768.0f);
    float pk = 20.0f * log10f((pkv + 1.0f) / 32768.0f);
    s_db = db;

    uint32_t now = millis();
    if (pk > peak || now - peak_since > 1000) {
      peak = pk;
      peak_since = now;
    }
    s_peak = peak;

    if (pk > CLAP_DB && now - last_clap > CLAP_GAP_MS) {
      last_clap = now;
      s_claps = s_claps + 1;
    }
#if MIC_DEBUG
    if (now - dbg > 500) {  // zur Fehlersuche: Rohwerte im seriellen Monitor
      dbg = now;
      printf("Mic: L %.0f R %.0f | Rohspitze %d | %.0f dB\r\n", rms_l, rms_r, pkv, db);
    }
#endif
  }
}

bool mic_begin() {
  if (s_ready) return true;
  s_mic.setPins(MIC_BCK, MIC_WS, -1, MIC_DIN);
  s_mic.setTimeout(1000);
  if (!s_mic.begin(I2S_MODE_STD, MIC_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    printf("Mikrofon: I2S-Start fehlgeschlagen\r\n");
    return false;
  }
  s_ready = true;
  xTaskCreatePinnedToCore(mic_task, "Mic", 4096, NULL, 1, NULL, 0);
  return true;
}

void mic_set_exclusive(bool on) {
  s_exclusive = on;
}

bool mic_exclusive() {
  return s_exclusive;
}

bool mic_idle() {
  return s_idle;
}

void mic_listen(bool on) {
  if (on) {
    mic_begin();
    s_last_data = millis();
  }
  s_listen = on;
  if (!on) {
    s_db = -90.0f;
    s_peak = -90.0f;
  }
}

float mic_level_db() { return s_db; }
float mic_peak_db() { return s_peak; }
int mic_channel() { return s_channel; }
int mic_raw_peak() { return s_raw_peak; }
bool mic_alive() { return millis() - s_last_data < 1500; }
int mic_restarts() { return s_restarts; }
int mic_claps() { return s_claps; }
void mic_reset_claps() { s_claps = 0; }

#else
// Host-Test: Mikrofon simulieren
bool mic_begin() { return false; }
void mic_listen(bool on) { (void)on; }
float mic_level_db() { return -52.0f; }
float mic_peak_db() { return -31.0f; }
int mic_channel() { return 0; }
int mic_raw_peak() { return 900; }
bool mic_alive() { return true; }
int mic_restarts() { return 0; }
void mic_set_exclusive(bool on) { (void)on; }
bool mic_exclusive() { return false; }
bool mic_idle() { return true; }
int mic_claps() { return 0; }
void mic_reset_claps() {}
#endif
