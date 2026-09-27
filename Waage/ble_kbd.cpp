#include "ble_kbd.h"
#include "config.h"

#if USE_BLE_KBD
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLE2902.h>
#include <HIDTypes.h>

static BLEHIDDevice *s_hid = nullptr;
static BLECharacteristic *s_input = nullptr;
static volatile bool s_connected = false;
static bool s_started = false;

// Standard-Tastatur mit Report-ID 1
static const uint8_t REPORT_MAP[] = {
  USAGE_PAGE(1), 0x01,        // Generic Desktop
  USAGE(1), 0x06,             // Keyboard
  COLLECTION(1), 0x01,        // Application
  REPORT_ID(1), 0x01,
  USAGE_PAGE(1), 0x07,        // Tasten
  USAGE_MINIMUM(1), 0xE0,
  USAGE_MAXIMUM(1), 0xE7,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1), 0x01,
  REPORT_COUNT(1), 0x08,
  HIDINPUT(1), 0x02,          // Modifier-Byte
  REPORT_COUNT(1), 0x01,
  REPORT_SIZE(1), 0x08,
  HIDINPUT(1), 0x01,          // reserviert
  REPORT_COUNT(1), 0x05,
  REPORT_SIZE(1), 0x01,
  USAGE_PAGE(1), 0x08,        // LEDs
  USAGE_MINIMUM(1), 0x01,
  USAGE_MAXIMUM(1), 0x05,
  HIDOUTPUT(1), 0x02,
  REPORT_COUNT(1), 0x01,
  REPORT_SIZE(1), 0x03,
  HIDOUTPUT(1), 0x01,
  REPORT_COUNT(1), 0x06,
  REPORT_SIZE(1), 0x08,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x65,
  USAGE_PAGE(1), 0x07,
  USAGE_MINIMUM(1), 0x00,
  USAGE_MAXIMUM(1), 0x65,
  HIDINPUT(1), 0x00,          // 6 Tasten
  END_COLLECTION(0)
};

class KbdCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    s_connected = true;
    BLE2902 *desc = (BLE2902 *)s_input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    if (desc) desc->setNotifications(true);
  }
  void onDisconnect(BLEServer *server) override {
    s_connected = false;
    server->startAdvertising();  // wieder sichtbar für den PC
  }
};

void ble_kbd_begin() {
  if (s_started) return;
  s_started = true;

  BLEDevice::init("Waage");
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new KbdCallbacks());

  s_hid = new BLEHIDDevice(server);
  s_input = s_hid->inputReport(1);
  s_hid->outputReport(1);
  s_hid->manufacturer()->setValue("Max Siebenschlaefer");
  s_hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  s_hid->hidInfo(0x00, 0x01);
  s_hid->reportMap((uint8_t *)REPORT_MAP, sizeof(REPORT_MAP));
  s_hid->startServices();
  s_hid->setBatteryLevel(100);

  BLESecurity *security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND);

  BLEAdvertising *adv = server->getAdvertising();
  adv->setAppearance(HID_KEYBOARD);
  adv->addServiceUUID(s_hid->hidService()->getUUID());
  adv->start();
}

bool ble_kbd_enabled() {
  return true;
}

bool ble_kbd_connected() {
  return s_connected;
}

static void send_key(uint8_t keycode) {
  uint8_t report[8] = { 0, 0, keycode, 0, 0, 0, 0, 0 };
  s_input->setValue(report, sizeof(report));
  s_input->notify();
  delay(8);
  memset(report, 0, sizeof(report));  // Taste loslassen
  s_input->setValue(report, sizeof(report));
  s_input->notify();
  delay(8);
}

bool ble_kbd_send_line(const char *digits) {
  if (!s_connected || !s_input) return false;
  for (const char *p = digits; *p; p++) {
    if (*p >= '1' && *p <= '9') send_key(0x1E + (*p - '1'));
    else if (*p == '0') send_key(0x27);
  }
  send_key(0x28);  // Enter
  return true;
}

#else
// Bluetooth ausgeschaltet: alles ohne Wirkung
void ble_kbd_begin() {}
bool ble_kbd_enabled() { return false; }
bool ble_kbd_connected() { return false; }
bool ble_kbd_send_line(const char *digits) { return false; }
#endif
