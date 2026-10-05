#include "ui_splash.h"
#include "ui_theme.h"
#include "ui_power.h"
#include "hal.h"

LV_IMG_DECLARE(img_splash_logo);
LV_IMG_DECLARE(img_splash_glow);
LV_IMG_DECLARE(img_splash_word);

// Zeitplan in Millisekunden ab dem ersten Bild
#define T_LIGHT_END   900   // Hintergrundbeleuchtung ist hochgefahren
#define T_GLOW_PEAK   450   // Leuchten am hellsten ...
#define T_GLOW_SETTLE 1400  // ... und dann auf den Ruhewert abgeklungen
#define T_LOGO_IN     100
#define T_LOGO_FULL   700
#define T_WORD_IN     750
#define T_WORD_FULL   1250
#define T_FADE_OUT    2500  // alles blendet zurück ins Schwarz
#define T_END         2900
#define GLOW_REST     90    // Deckkraft des Leuchtens nach dem Aufblühen
#define WORD_RISE     12    // Schriftzug gleitet beim Einblenden um so viele Pixel nach oben

static lv_obj_t *s_scr, *s_glow, *s_logo, *s_word;
static lv_timer_t *s_timer;
static uint32_t s_t0;
static void (*s_done)();

// 0..1 zwischen a und b, mit sanftem Auslauf
static float ramp(uint32_t t, uint32_t a, uint32_t b) {
  if (t <= a) return 0.0f;
  if (t >= b) return 1.0f;
  float x = (float)(t - a) / (float)(b - a);
  return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);  // ease-out
}

static lv_opa_t opa(float f) {
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  return (lv_opa_t)(f * 255.0f + 0.5f);
}

static void splash_tick(lv_timer_t *tm) {
  uint32_t t = hal_millis() - s_t0;
  float fade = 1.0f - ramp(t, T_FADE_OUT, T_END);

  // Hintergrundbeleuchtung aus dem Schwarz hochfahren (quadratisch, wirkt fürs Auge gleichmäßiger)
  float l = ramp(t, 0, T_LIGHT_END);
  hal_backlight((int)(BRIGHT_ON * l * l + 0.5f));

  // Leuchten blüht auf und klingt auf einen Ruhewert ab
  float g = ramp(t, 0, T_GLOW_PEAK);
  if (t > T_GLOW_PEAK) g = 1.0f - (1.0f - GLOW_REST / 255.0f) * ramp(t, T_GLOW_PEAK, T_GLOW_SETTLE);
  lv_obj_set_style_img_opa(s_glow, opa(g * fade), 0);

  lv_obj_set_style_img_opa(s_logo, opa(ramp(t, T_LOGO_IN, T_LOGO_FULL) * fade), 0);

  float w = ramp(t, T_WORD_IN, T_WORD_FULL);
  lv_obj_set_style_img_opa(s_word, opa(w * fade), 0);
  lv_obj_set_style_translate_y(s_word, (lv_coord_t)(WORD_RISE * (1.0f - w)), 0);

  if (t >= T_END) {
    lv_timer_del(s_timer);
    s_timer = NULL;
    hal_backlight(BRIGHT_ON);
    void (*done)() = s_done;
    s_done = NULL;
    s_scr = NULL;      // done() lädt den nächsten Bildschirm und löscht dabei das Startbild
    if (done) done();
  }
}

static lv_obj_t *add_img(lv_obj_t *parent, const lv_img_dsc_t *src, lv_color_t col) {
  lv_obj_t *o = lv_img_create(parent);
  lv_img_set_src(o, src);
  // Bilder enthalten nur die Deckkraft, die Farbe kommt aus dem Farbschema
  lv_obj_set_style_img_recolor(o, col, 0);
  lv_obj_set_style_img_recolor_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_img_opa(o, LV_OPA_TRANSP, 0);
  return o;
}

void ui_splash_start(void (*done)()) {
  s_done = done;
  hal_backlight(0);
  s_scr = ui_screen_create();

  s_glow = add_img(s_scr, &img_splash_glow, C_ACCENT);
  lv_obj_align(s_glow, LV_ALIGN_CENTER, 0, -28);
  s_logo = add_img(s_scr, &img_splash_logo, C_ACCENT);
  lv_obj_align(s_logo, LV_ALIGN_CENTER, 0, -28);
  s_word = add_img(s_scr, &img_splash_word, C_TEXT);
  lv_obj_align(s_word, LV_ALIGN_CENTER, 0, 104);

  lv_scr_load_anim(s_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
  lv_refr_now(NULL);  // erstes Bild (schwarz) zeichnen, bevor das Licht angeht
  s_t0 = hal_millis();
  s_timer = lv_timer_create(splash_tick, 16, NULL);
}
