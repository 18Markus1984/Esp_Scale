#pragma once
// ============================================================
//  Texteingabe über den Buchstabenring (siehe Design-Sheet)
//  Finger am Rand entlangführen, Lupe in der Mitte zeigt den
//  Buchstaben, Loslassen übernimmt. Zum Abbrechen eines Zeichens
//  den Finger vor dem Loslassen in die Mitte ziehen.
// ============================================================
#include <lvgl.h>

// ok = true bei "OK", false bei "Abbrechen"
typedef void (*ui_text_done_cb)(bool ok, const char *text);

// title:    kleine Zeile oben, z. B. "Neuer Topf · Name"
// password: keine automatische Großschreibung, Start mit Kleinbuchstaben
// suggest:  Vorschlagsliste (darf NULL sein)
lv_obj_t *ui_text_page_create(const char *title, const char *initial, int max_len, bool password,
                              const char *const *suggest, int n_suggest, ui_text_done_cb cb);
