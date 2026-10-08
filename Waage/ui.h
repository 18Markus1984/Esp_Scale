#pragma once
// ============================================================
//  UI-Einstieg: Lagecheck -> Wiegen <-> Modi <-> System
// ============================================================
#include <lvgl.h>

// Nach Lvgl_Init() aufrufen
void ui_init();

// Aus Unterseiten zurück zur Startseite (Wiegen/Modi/System)
void ui_go_home();
// Wischen nach rechts führt auf der aktuellen Seite zu fn() statt zur Wiegeseite
// (nach ui_switch_page() aufrufen; gilt für einmal Wischen)
void ui_set_back(lv_obj_t *(*fn)());

// Unterseite öffnen (wird beim Verlassen automatisch gelöscht)
void ui_open_page(lv_obj_t *page);

// Innerhalb eines Modus zum nächsten Schritt wechseln
// (ersetzt die aktuelle Seite, Wischen nach rechts führt weiterhin heim)
void ui_switch_page(lv_obj_t *page);

// Topf aus der Liste abziehen (gefüllter Topf) und zur Wiegeseite wechseln
void ui_pot_subtract(int idx);

// Nach Änderungen an der Topfliste aufrufen
void ui_pots_changed();

// Drückt auf der aktuellen Seite den ersten Knopf, dessen Beschriftung
// in der Liste steht (für Sprachbefehle). true = gefunden
bool ui_click_button(const char *const labels[], int count);

// Touch nach einem Seitenwechsel kurz sperren (verhindert Durchklicken)
void ui_input_lock();

// Nach dem Umstellen der Sprache aufrufen: baut die Startseite neu auf
void ui_lang_changed();
void ui_theme_changed();  // Farbschema umgestellt: Startseite neu aufbauen

// Name des aktiven Topfs ("" = keiner), z. B. für die Weboberfläche
const char *ui_active_pot();
