#pragma once
// ============================================================
//  Feinausrichtung des Displays: das ganze Bild um wenige Grad drehen
//
//  Gleicht ein leicht verdreht eingebautes Display aus. LVGL zeichnet
//  dafür in ein eigenes Vollbild (PSRAM); beim Ausgeben wird nur der
//  geänderte Bereich gedreht (bilinear) und an den Original-Treiber
//  gegeben. Touch-Punkte werden passend zurückgedreht.
//  Bei 0,0° wird nichts umgebaut, alles bleibt wie vorher.
// ============================================================
#include <stdint.h>

#define DISP_ROT_MAX 50   // ±5,0° (Einheit: Zehntelgrad)

void disp_rot_set(int tenths);  // sofort anwenden (installiert sich beim ersten Mal)
int  disp_rot_get();
void disp_rot_suspend();         // Originaltreiber zurück (z. B. während des Online-Updates)
void disp_rot_resume();          // eingestellte Drehung wieder anwenden
