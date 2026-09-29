// Host-Ersatz für HAL und Mikrofon (nur Testumgebung)
#include "hal.h"
#include <stdint.h>
uint32_t g_ms = 0;
float g_ax = 0, g_ay = 0, g_az = 1;
void hal_init() {}
uint32_t hal_millis() { return g_ms; }
bool hal_accel(float *x, float *y, float *z) { *x = g_ax; *y = g_ay; *z = g_az; return true; }
bool hal_time(int *h, int *m) { *h = 14; *m = 32; return true; }
bool hal_now(int *y, int *mo, int *d, int *h, int *mi, int *s) { *y = 2026; *mo = 9; *d = 26; *h = 14; *mi = 32; *s = 5; return true; }
void hal_set_datetime(int, int, int, int, int, int) {}
int hal_battery_percent() { return 72; }
float hal_battery_volts() { return 3.88f; }
bat_state_t hal_battery_state() { return BAT_DISCHARGE; }
bool hal_boot_pressed() { return false; }
void hal_power_hold(bool) {}
bool hal_pwr_pressed() { return false; }
void hal_backlight(int) {}
void hal_restart() {}
bool mic_begin() { return true; }
void mic_set_exclusive(bool) {}
bool mic_exclusive() { return false; }
bool mic_idle() { return true; }
void mic_listen(bool) {}
float mic_level_db() { return -42; }
float mic_peak_db() { return -30; }
int mic_claps() { return 0; }
int mic_channel() { return 0; }
int mic_raw_peak() { return 1200; }
bool mic_alive() { return true; }
int mic_restarts() { return 0; }
void mic_reset_claps() {}
