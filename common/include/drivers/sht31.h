// sht31.h - Own bus-level I2C driver for Sensirion SHT31 (temp + humidity).
// No third-party library: we send the raw 16-bit command and decode the bytes.
// Datasheet: single-shot, high repeatability, no clock stretching = 0x2400.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "sensirion_crc.h"

class Sht31 {
public:
  explicit Sht31(uint8_t addr = 0x44) : addr_(addr) {}

  bool begin(TwoWire &wire = Wire) {
    wire_ = &wire;
    return sendCommand(0x30A2); // soft reset
  }

  // Blocking single-shot read (~15 ms). Returns false on NACK or CRC error.
  bool read(float &tempC, float &rh) {
    if (!sendCommand(0x2400)) return false;
    delay(16); // max measurement time for high repeatability
    uint8_t buf[6];
    if (wire_->requestFrom(addr_, (uint8_t)6) != 6) return false;
    for (int i = 0; i < 6; i++) buf[i] = wire_->read();
    // Each 16-bit word is followed by its CRC byte
    if (sensirionCrc8(buf, 2) != buf[2] || sensirionCrc8(buf + 3, 2) != buf[5]) return false;
    uint16_t rawT = (buf[0] << 8) | buf[1];
    uint16_t rawH = (buf[3] << 8) | buf[4];
    tempC = -45.0f + 175.0f * rawT / 65535.0f;
    rh = 100.0f * rawH / 65535.0f;
    return true;
  }

private:
  bool sendCommand(uint16_t cmd) {
    wire_->beginTransmission(addr_);
    wire_->write(cmd >> 8);
    wire_->write(cmd & 0xFF);
    return wire_->endTransmission() == 0;
  }
  uint8_t addr_;
  TwoWire *wire_ = nullptr;
};
