// scd4x.h - Own bus-level I2C driver for Sensirion SCD40/SCD41 (CO2).
// Uses periodic measurement mode: a new sample every 5 s.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "sensirion_crc.h"

class Scd4x {
public:
  static constexpr uint8_t ADDR = 0x62;

  bool begin(TwoWire &wire = Wire) {
    wire_ = &wire;
    sendCommand(0x3F86); // stop_periodic_measurement (in case it was running)
    delay(500);          // datasheet: wait 500 ms after stop
    return sendCommand(0x21B1); // start_periodic_measurement
  }

  // True when a new measurement is waiting (get_data_ready_status 0xE4B8).
  bool dataReady() {
    uint16_t word;
    if (!sendCommand(0xE4B8)) return false;
    delay(1);
    if (!readWords(&word, 1)) return false;
    return (word & 0x07FF) != 0; // lower 11 bits != 0 means ready
  }

  // read_measurement 0xEC05 -> CO2 [ppm], temperature, humidity
  bool read(uint16_t &co2, float &tempC, float &rh) {
    if (!sendCommand(0xEC05)) return false;
    delay(1);
    uint16_t w[3];
    if (!readWords(w, 3)) return false;
    co2 = w[0];
    tempC = -45.0f + 175.0f * w[1] / 65535.0f;
    rh = 100.0f * w[2] / 65535.0f;
    return co2 != 0;
  }

private:
  bool sendCommand(uint16_t cmd) {
    wire_->beginTransmission(ADDR);
    wire_->write(cmd >> 8);
    wire_->write(cmd & 0xFF);
    return wire_->endTransmission() == 0;
  }
  // Read n 16-bit words, each followed by a CRC byte.
  bool readWords(uint16_t *out, int n) {
    int len = n * 3;
    if (wire_->requestFrom(ADDR, (uint8_t)len) != len) return false;
    for (int i = 0; i < n; i++) {
      uint8_t b[3] = {(uint8_t)wire_->read(), (uint8_t)wire_->read(), (uint8_t)wire_->read()};
      if (sensirionCrc8(b, 2) != b[2]) return false;
      out[i] = (b[0] << 8) | b[1];
    }
    return true;
  }
  TwoWire *wire_ = nullptr;
};
