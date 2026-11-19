// sensirion_crc.h - CRC-8 used by Sensirion sensors (SHT3x, SCD4x).
// Polynomial 0x31 (x^8 + x^5 + x^4 + 1), init 0xFF, no final XOR.
#pragma once
#include <stdint.h>

inline uint8_t sensirionCrc8(const uint8_t *data, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}
