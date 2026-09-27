#pragma once
// ============================================================
//  Sonder-Widgets: Kurvenliste und Libelle (Blase)
// ============================================================
#include <lvgl.h>

typedef struct {
  const char *title;
  const char *sub;   // kleine Zeile unter dem markierten Eintrag, darf NULL sein
  const char *icon;  // optional: Symbol aus font_icons_26 (ICON_... in ui_theme.h)
} ui_list_item_t;

// Wird aufgerufen, wenn der markierte Eintrag angetippt wird
typedef void (*ui_list_cb_t)(int index);

// Kurvenliste: vertikal scrollen, der Eintrag in der Mitte ist markiert.
// Tippen auf einen anderen Eintrag scrollt ihn in die Mitte.
lv_obj_t *ui_curved_list(lv_obj_t *parent, const ui_list_item_t *items, int count, ui_list_cb_t cb);

// Libelle mit Fadenkreuz und Blase
lv_obj_t *ui_bubble_create(lv_obj_t *parent, int size);
// dx/dy in Grad; bei 6° erreicht die Blase den Rand
void ui_bubble_set(lv_obj_t *bubble, float dx_deg, float dy_deg, lv_color_t color);

// Neigung aus der IMU: Gesamtwinkel und Richtung in Grad.
// false = keine gültigen Sensordaten
bool ui_read_tilt(float *tilt_deg, float *dx_deg, float *dy_deg);
// ohne gespeicherten Nullpunkt (für die Kalibrierung der Libelle)
bool ui_read_tilt_raw(float *dx_deg, float *dy_deg);
