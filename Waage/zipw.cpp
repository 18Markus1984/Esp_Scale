#include "zipw.h"
#include <string.h>

// CRC-32 wie zlib (Polynom 0xEDB88320), mit 16er-Tabelle: klein und schnell genug
uint32_t zip_crc32(uint32_t crc, const uint8_t *buf, size_t len) {
  static const uint32_t T[16] = { 0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
                                  0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
                                  0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C };
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc ^= buf[i];
    crc = (crc >> 4) ^ T[crc & 15];
    crc = (crc >> 4) ^ T[crc & 15];
  }
  return ~crc;
}

static void p16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void p32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

#define ZFLAGS 0x0808  // Bit 3: Datenbeschreibung hinter den Daten, Bit 11: Namen in UTF-8

int zip_local_header(uint8_t *o, const char *name, uint16_t t, uint16_t d) {
  int n = (int)strlen(name);
  p32(o, 0x04034b50);
  p16(o + 4, 20);      // Version
  p16(o + 6, ZFLAGS);
  p16(o + 8, 0);       // gespeichert, ohne Kompression
  p16(o + 10, t);
  p16(o + 12, d);
  p32(o + 14, 0);      // CRC, Größen: folgen in der Datenbeschreibung
  p32(o + 18, 0);
  p32(o + 22, 0);
  p16(o + 26, n);
  p16(o + 28, 0);
  memcpy(o + 30, name, n);
  return 30 + n;
}

int zip_data_desc(uint8_t *o, uint32_t crc, uint32_t size) {
  p32(o, 0x08074b50);
  p32(o + 4, crc);
  p32(o + 8, size);
  p32(o + 12, size);
  return 16;
}

int zip_central(uint8_t *o, const char *name, uint32_t crc, uint32_t size, uint32_t off, uint16_t t, uint16_t d) {
  int n = (int)strlen(name);
  p32(o, 0x02014b50);
  p16(o + 4, 20);
  p16(o + 6, 20);
  p16(o + 8, ZFLAGS);
  p16(o + 10, 0);
  p16(o + 12, t);
  p16(o + 14, d);
  p32(o + 16, crc);
  p32(o + 20, size);
  p32(o + 24, size);
  p16(o + 28, n);
  p16(o + 30, 0);
  p16(o + 32, 0);
  p16(o + 34, 0);
  p16(o + 36, 0);
  p32(o + 38, 0);
  p32(o + 42, off);
  memcpy(o + 46, name, n);
  return 46 + n;
}

int zip_end(uint8_t *o, uint16_t entries, uint32_t cd_size, uint32_t cd_off) {
  p32(o, 0x06054b50);
  p16(o + 4, 0);
  p16(o + 6, 0);
  p16(o + 8, entries);
  p16(o + 10, entries);
  p32(o + 12, cd_size);
  p32(o + 16, cd_off);
  p16(o + 20, 0);
  return 22;
}

void zip_dos_time(int year, int mon, int day, int h, int m, int s, uint16_t *t, uint16_t *d) {
  if (year < 1980) {  // Uhr nicht gestellt: 1.1.2026
    year = 2026;
    mon = day = 1;
    h = m = s = 0;
  }
  *t = (uint16_t)((h << 11) | (m << 5) | (s / 2));
  *d = (uint16_t)(((year - 1980) << 9) | (mon << 5) | day);
}
