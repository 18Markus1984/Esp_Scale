#pragma once
// ============================================================
//  Startbild: Logo und Schriftzug „esp scale“ leuchten aus dem
//  schwarzen Display auf. Die Hintergrundbeleuchtung bleibt bis
//  zum ersten fertigen Bild aus, damit beim Einschalten keine
//  Reste oder Testbilder des Display-Controllers zu sehen sind.
// ============================================================
#include <lvgl.h>

// Zeigt das Startbild und ruft danach done() auf (aus dem LVGL-Timer).
void ui_splash_start(void (*done)());
