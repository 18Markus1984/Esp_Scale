#pragma once
// ============================================================
//  Bluetooth-Tastatur: Die Waage meldet sich als "Waage" am PC an
//  und tippt Zahlen in das gerade aktive Feld (z. B. Excel-Zelle).
//  Es werden nur Ziffern und Enter gesendet, dadurch ist das
//  Tastaturlayout am PC (QWERTZ/QWERTY) egal.
// ============================================================

bool ble_kbd_begin();                       // Bluetooth starten (mehrfacher Aufruf ist ok), false = zu wenig Speicher
bool ble_kbd_no_memory();                   // letzter Start abgelehnt: zu wenig interner RAM
bool ble_kbd_enabled();                     // in config.h eingeschaltet?
bool ble_kbd_connected();                   // PC gekoppelt und verbunden?
bool ble_kbd_send_line(const char *digits); // Ziffern tippen + Enter
