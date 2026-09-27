// ============================================================
//  Waage – Schritt 1: Oberfläche
//  Basis: Waveshare-Demo LVGL_Arduino (ESP32-S3-Touch-LCD-1.46)
//  Arduino-ESP32 3.1.1, LVGL 8.3.10
// ============================================================
#include "Display_SPD2010.h"
#include "RTC_PCF85063.h"
#include "Gyro_QMI8658.h"
#include "LVGL_Driver.h"
#include "PWR_Key.h"
#include "BAT_Driver.h"
#include "ui.h"
#include "hal.h"

// Sensoren laufen in einem eigenen Task auf Kern 0,
// die Oberfläche läuft in loop() auf Kern 1.
void Driver_Loop(void *parameter) {
  uint8_t slow = 0;
  while (1) {
    // PWR-Taste wertet die Oberfläche aus (ui_power.cpp), nicht mehr PWR_Loop()
    QMI8658_Loop();          // Lage, alle 50 ms für eine ruhige Libelle
    if (++slow >= 10) {      // Uhr und Akku reichen alle 500 ms
      slow = 0;
      PCF85063_Loop();
      BAT_Get_Volts();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void Driver_Init() {
  BAT_Init();
  I2C_Init();
  TCA9554PWR_Init(0x00);
  // Display und Touch vor dem Initialisieren länger im Reset halten.
  // Nach einem Flash oder Neustart hat der Display-Controller sonst noch
  // den alten Zustand und das Bild ist verzerrt (nur Strom trennen half).
  Set_EXIO(EXIO_PIN2, Low);
  Set_EXIO(EXIO_PIN1, Low);
  delay(200);
  Backlight_Init();
  Set_Backlight(60);  // 0..100
  PCF85063_Init();
  QMI8658_Init();
}

void setup() {
  // Als Allererstes: Akku eingeschaltet halten. Sonst geht die Waage aus,
  // sobald die PWR-Taste nach dem Einschalten losgelassen wird.
  hal_power_hold(true);
  Driver_Init();
  LCD_Init();
  Lvgl_Init();
  ui_init();

  xTaskCreatePinnedToCore(Driver_Loop, "Driver", 4096, NULL, 3, NULL, 0);
}

void loop() {
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(5));
}
