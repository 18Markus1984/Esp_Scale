#pragma once
// ============================================================
//  ZIP ohne Kompression zum Herunterladen ganzer Ordner
//  (Weboberfläche: Dateien -> Ordner als ZIP). Die Teile werden
//  nacheinander geschickt; Prüfsumme und Größe stehen jeweils
//  hinter den Daten (Datenbeschreibung), deshalb muss nichts
//  vorher zwischengespeichert werden. Bis 4 GB, ohne ZIP64.
// ============================================================
#include <stdint.h>
#include <stddef.h>

uint32_t zip_crc32(uint32_t crc, const uint8_t *buf, size_t len);  // Start mit crc = 0
// Bausteine, Rückgabe = geschriebene Bytes (out braucht 46 + Namenslänge)
int zip_local_header(uint8_t *out, const char *name, uint16_t dos_time, uint16_t dos_date);
int zip_data_desc(uint8_t *out, uint32_t crc, uint32_t size);
int zip_central(uint8_t *out, const char *name, uint32_t crc, uint32_t size, uint32_t offset,
                uint16_t dos_time, uint16_t dos_date);
int zip_end(uint8_t *out, uint16_t entries, uint32_t cd_size, uint32_t cd_offset);
// DOS-Zeitstempel aus Datum und Uhrzeit
void zip_dos_time(int year, int mon, int day, int h, int m, int s, uint16_t *t, uint16_t *d);
