#pragma once
// ============================================================
//  Farben, Schriften und wiederverwendbare UI-Bausteine
// ============================================================
#include <lvgl.h>
#include <stddef.h>
#include "i18n.h"  // Übersetzung: lv_label_set_text() übersetzt ab hier automatisch

// Schriften (Space Grotesk Medium, inkl. Umlaute und ° ± · ‹ ›)
LV_FONT_DECLARE(font_sg_14);
LV_FONT_DECLARE(font_sg_18);
LV_FONT_DECLARE(font_sg_24);
LV_FONT_DECLARE(font_sg_34);
// Symbole (Material Symbols Rounded), als UTF-8
#define ICON_KUECHE    "\xEE\xAD\x87"  // kitchen
#define ICON_WERKSTATT "\xEF\x84\x8B"  // handyman
#define ICON_SPIELE    "\xEE\xA8\xA8"  // sports_esports
#define ICON_REZEPT    "\xEE\x95\xA1"  // restaurant_menu
#define ICON_COCKTAIL  "\xEE\x95\x80"  // local_bar
#define ICON_ZIEL      "\xEF\x83\x86"  // flag
#define ICON_SPULE     "\xEE\xA4\x97"  // donut_large
#define ICON_ZAEHLEN   "\xEE\xAB\x87"  // numbers
#define ICON_PORTO     "\xEE\x85\x99"  // mail
#define ICON_SPIEL     "\xEE\xAD\x80"  // casino
#define ICON_TRINK     "\xEF\x87\xB3"  // sports_bar
#define ICON_PROTOKOLL "\xEE\xBD\xAE"  // receipt_long
#define ICON_TOEPFE    "\xEE\x9F\x93"  // soup_kitchen
#define ICON_LIBELLE   "\xEE\x90\x9C"  // straighten
#define ICON_KALIB     "\xEE\x90\xA9"  // tune
#define ICON_MIKRO     "\xEE\x8C\x9D"  // mic
#define ICON_SETUP     "\xEE\xA2\xB8"  // settings
#define ICON_WIEGEN    "\xEF\x80\xB9"  // monitor_weight
#define ICON_ZEIT      "\xEE\xBF\x96"  // schedule
#define ICON_TON       "\xEE\x81\x90"  // volume_up
#define ICON_WAAGE     "\xEE\xAD\x9F"  // scale
#define ICON_AKKU      "\xEE\x86\xA3"  // battery_charging_full
#define ICON_PORTION   "\xEE\xA9\x93"  // bakery_dining (U+EA53)
#define ICON_TIMER     "\xEE\x90\xA5"  // timer (U+E425)
#define ICON_MIX       "\xEE\xA9\x8B"  // science (U+EA4B)
#define ICON_LANGZEIT  "\xEF\x86\x90"  // monitoring (U+F190)
#define ICON_TASSE     "\xEE\xBF\xAF"  // coffee (U+EFEF)
#define ICON_MUENZEN   "\xEE\x8B\xAB"  // savings (U+E2EB)
#define ICON_BLIND     "\xEE\x9E\x98"  // water_drop (U+E798)
#define ICON_HALB      "\xEE\x85\x8E"  // content_cut (U+E14E)
#define ICON_TON_AUS   "\xEE\x81\x8F"  // volume_off (U+E04F)
#define ICON_SPRUECHE  "\xEE\xA4\x9F"  // record_voice_over (U+E91F)

LV_FONT_DECLARE(font_icons_26);  // Material Symbols, nur die benutzten Zeichen
LV_FONT_DECLARE(font_sg_80);  // Ziffern, % und , . - : ? (Gewicht, Uhr)
LV_FONT_DECLARE(font_sg_104); // dieselben Zeichen, groß für die Wiegeseite

// Farben passend zum Gehäuse: schwarzer Rahmen, weiße Bedienfläche.
// Weiß ist die Bedienfarbe (Knöpfe, Auswahl), Grün nur für Messwert und Zustand
// (Ring, "stabil", Schalter an), Gelb/Rot nur für Warnungen.
#define C_BG        lv_color_hex(0x090909)
#define C_TEXT      lv_color_hex(0xF4F4F1)
#define C_TEXT2     lv_color_hex(0xC4C4C0)
#define C_MUTED     lv_color_hex(0x9C9C98)
#define C_FAINT     lv_color_hex(0x62625F)
#define C_SURFACE   lv_color_hex(0x181818)
#define C_BORDER    lv_color_hex(0x3A3A3A)
#define C_TRACK     lv_color_hex(0x232323)
#define C_PRIMARY   lv_color_hex(0xF4F4F1)
#define C_ON_PRIMARY lv_color_hex(0x0A0A0A)
#define C_ACCENT    lv_color_hex(0x3DDC97)
#define C_ON_ACCENT lv_color_hex(0x0A0A0A)
#define C_WARN      lv_color_hex(0xF5B83D)
#define C_DANGER    lv_color_hex(0xFF6B5E)

#define SCREEN_SIZE 412

typedef enum { BTN_NORMAL, BTN_PRIMARY, BTN_WARN } ui_btn_kind_t;

lv_obj_t *ui_screen_create();
lv_obj_t *ui_box(lv_obj_t *parent);
lv_obj_t *ui_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_btn(lv_obj_t *parent, const char *txt, ui_btn_kind_t kind);
lv_obj_t *ui_chip(lv_obj_t *parent, const char *txt, lv_color_t color);
void      ui_chip_set(lv_obj_t *chip, const char *txt, lv_color_t color, bool filled);
lv_obj_t *ui_ring(lv_obj_t *parent, int size);
lv_obj_t *ui_page_dots(lv_obj_t *parent, int count, int active);

// Runder Knopf mit großem Zeichen, z. B. für + und −
lv_obj_t *ui_round_btn(lv_obj_t *parent, const char *txt, lv_event_cb_t cb);

// Timer, der automatisch mit der Seite gelöscht wird
lv_timer_t *ui_page_timer(lv_obj_t *page, lv_timer_cb_t cb, uint32_t period_ms);

// Kurze Meldung, verschwindet nach 2 s von selbst
lv_obj_t *ui_toast_create(lv_obj_t *parent);
void      ui_toast_show(lv_obj_t *toast, const char *txt, lv_color_t color);

// Label-Text nur setzen, wenn er sich geändert hat (spart Neuzeichnen)
void ui_label_update(lv_obj_t *label, const char *txt);

// 1234.5 -> "1234,5"
void ui_fmt_weight(char *buf, size_t len, float grams);
// 0.4 -> "0,4"
void ui_fmt_1dec(char *buf, size_t len, float value);
