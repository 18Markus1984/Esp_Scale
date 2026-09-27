#include "i18n.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Beim ersten Aufruf wird ein sortiertes Verzeichnis angelegt,
// danach findet die binäre Suche jeden Text in ~10 Vergleichen.
static const i18n_pair_t **s_index = NULL;

static int cmp_pair(const void *a, const void *b) {
  return strcmp((*(const i18n_pair_t *const *)a)->de, (*(const i18n_pair_t *const *)b)->de);
}

static void build_index() {
  s_index = (const i18n_pair_t **)malloc(sizeof(i18n_pair_t *) * I18N_EN_COUNT);
  if (!s_index) return;
  for (int i = 0; i < I18N_EN_COUNT; i++) s_index[i] = &I18N_EN[i];
  qsort(s_index, I18N_EN_COUNT, sizeof(i18n_pair_t *), cmp_pair);
}

#ifndef ARDUINO
// Testumgebung: nicht übersetzte Texte sammeln (siehe Testumgebung/LIESMICH.md)
void i18n_miss(const char *de);
#endif

const char *T(const char *de) {
  if (!de || !de[0] || g_set.lang != LANG_EN) return de;
  if (!s_index) build_index();
  if (!s_index) return de;
  int lo = 0, hi = I18N_EN_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int c = strcmp(de, s_index[mid]->de);
    if (c == 0) return s_index[mid]->en;
    if (c < 0) hi = mid - 1;
    else lo = mid + 1;
  }
#ifndef ARDUINO
  i18n_miss(de);
#endif
  return de;
}

#ifndef ARDUINO
void i18n_shown(const char *txt);  // Testumgebung: alle angezeigten Texte mitschreiben
#endif

void i18n_label_set_text(lv_obj_t *label, const char *txt) {
  if (txt) txt = T(txt);
#ifndef ARDUINO
  if (txt) i18n_shown(txt);
#endif
  (lv_label_set_text)(label, txt);  // Klammern: echte LVGL-Funktion
}

void i18n_label_set_text_fmt(lv_obj_t *label, const char *fmt, ...) {
  char b[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof(b), T(fmt), ap);
  va_end(ap);
#ifndef ARDUINO
  i18n_shown(b);
#endif
  (lv_label_set_text)(label, b);
}
