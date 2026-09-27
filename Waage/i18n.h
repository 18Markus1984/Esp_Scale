#pragma once
// ============================================================
//  Sprache der Oberfläche (Deutsch / English)
//
//  Alle Texte im Code bleiben Deutsch. Ist Englisch eingestellt,
//  sucht T() den deutschen Text in der Tabelle i18n_en.cpp und
//  liefert die Übersetzung; fehlt ein Eintrag, bleibt es Deutsch.
//
//  Damit nicht jeder Aufruf angepasst werden muss, übersetzt schon
//  lv_label_set_text() selbst (Makro unten). Feste Texte wie
//  ui_label(s, "Speichern", ...) oder Listeneinträge brauchen also
//  nichts weiter. T() ist nur nötig, wenn Text erst zusammengesetzt
//  wird, z. B. snprintf(b, n, T("noch %d g"), rest).
//
//  Neuer Text: in i18n_en.cpp eine Zeile { "Deutsch", "English" }
//  ergänzen. Die Reihenfolge ist egal.
// ============================================================
#include <lvgl.h>
#include "settings.h"

typedef struct {
  const char *de;
  const char *en;
} i18n_pair_t;

// Übersetzungstabelle (i18n_en.cpp)
extern const i18n_pair_t I18N_EN[];
extern const int I18N_EN_COUNT;

// Übersetzt einen deutschen Text in die eingestellte Sprache
const char *T(const char *de);

// true, wenn Englisch eingestellt ist
static inline bool lang_en() { return g_set.lang == LANG_EN; }

// Ersatz für lv_label_set_text / _fmt, der vorher übersetzt
void i18n_label_set_text(lv_obj_t *label, const char *txt);
void i18n_label_set_text_fmt(lv_obj_t *label, const char *fmt, ...);

// Ab hier übersetzt jedes lv_label_set_text() automatisch.
// (In i18n.cpp wird die echte LVGL-Funktion als (lv_label_set_text)(...) aufgerufen.)
#define lv_label_set_text(label, txt) i18n_label_set_text((label), (txt))
#define lv_label_set_text_fmt(label, ...) i18n_label_set_text_fmt((label), __VA_ARGS__)
