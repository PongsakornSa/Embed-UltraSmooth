// bh1750.h - Own I2C driver for ROHM BH1750 ambient light sensor.
#pragma once
#include <Arduino.h>
#include <Wire.h>

class Bh1750 {
public:
  explicit Bh1750(uint8_t addr = 0x23) : addr_(addr) {}

  bool begin(TwoWire &wire = Wire) {
    wire_ = &wire;
    if (!write(0x01)) return false; // power on
    return write(0x10);             // continuous high-resolution mode (1 lx, 120 ms)
  }

  bool read(float &lux) {
    if (wire_->requestFrom(addr_, (uint8_t)2) != 2) return false;
    uint16_t raw = (wire_->read() << 8) | wire_->read();
    lux = raw / 1.2f; // datasheet conversion factor
    return true;
  }

private:
  bool write(uint8_t op) {
    wire_->beginTransmission(addr_);
    wire_->write(op);
    return wire_->endTransmission() == 0;
  }
  uint8_t addr_;
  TwoWire *wire_ = nullptr;
};
