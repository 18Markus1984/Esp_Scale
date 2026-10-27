#pragma once
// ============================================================
//  Hilfe zur Sprachsteuerung (was kann man sagen?)
//
//  - Nach „Hallo Waage“ (bzw. Doppeltipp) legt sich für die Zeit des
//    Zuhörens ein Hinweis über die Seite: „Ich höre zu …“ und drei
//    Beispiele, passend zur Seite (Zielwiegen, Rezept mit „Weiter“, sonst
//    Wiegen). Danach kurz, was verstanden wurde.
//  - Seite „Sprachbefehle“ (Setup -> Ton -> Befehle): alle Befehle mit
//    Erklärung. Der Sprachbefehl „Hilfe“ zeigt dieselbe Liste über der
//    aktuellen Seite (die bleibt erhalten).
// ============================================================
#include <lvgl.h>

void voice_help_tick();              // im UI-Takt: Hinweis nach dem Weckwort ein-/ausblenden
void voice_help_show_all();          // Liste aller Befehle über der aktuellen Seite (Befehl „Hilfe“)
void voice_help_block(bool on);      // Mikrofonseite offen: dort keinen Hinweis (sie zeigt selbst an)
lv_obj_t *page_voice_cmds_create();  // Setup -> Ton -> Befehle

// Knöpfe, die der Befehl „Weiter“ drückt (voice.cpp) – und an denen der Hinweis erkennt,
// dass „Weiter“ auf der Seite gerade geht
extern const char *const VOICE_NEXT_LABELS[];
extern const int VOICE_NEXT_COUNT;
