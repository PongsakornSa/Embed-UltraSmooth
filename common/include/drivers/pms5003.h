// pms5003.h - Own UART protocol driver for Plantower PMS5003 (PM2.5 / PM10).
// Frame (32 bytes): 0x42 0x4D, len(2)=28, 13 data words, checksum(2).
// Checksum = sum of all bytes before the checksum field.
#pragma once
#include <Arduino.h>

class Pms5003 {
public:
  void begin(HardwareSerial &serial, int rxPin, int txPin) {
    serial_ = &serial;
    serial_->begin(9600, SERIAL_8N1, rxPin, txPin);
  }

  // Non-blocking: feed available bytes into the parser.
  // Returns true when a complete, valid frame was decoded.
  bool poll() {
    while (serial_->available()) {
      uint8_t b = serial_->read();
      if (idx_ == 0 && b != 0x42) continue;
      if (idx_ == 1 && b != 0x4D) { idx_ = 0; continue; }
      buf_[idx_++] = b;
      if (idx_ == 32) {
        idx_ = 0;
        if (decode()) return true;
      }
    }
    return false;
  }

  // Atmospheric-environment values (use these for outdoor air)
  uint16_t pm1() const { return pm1_; }
  uint16_t pm25() const { return pm25_; }
  uint16_t pm10() const { return pm10_; }
  uint32_t lastFrameMs() const { return lastMs_; }

private:
  bool decode() {
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += buf_[i];
    uint16_t check = be16(30);
    if (sum != check || be16(2) != 28) return false;
    pm1_ = be16(10);  // PM1.0 atmospheric
    pm25_ = be16(12); // PM2.5 atmospheric
    pm10_ = be16(14); // PM10 atmospheric
    lastMs_ = millis();
    return true;
  }
  uint16_t be16(int i) const { return (buf_[i] << 8) | buf_[i + 1]; }

  HardwareSerial *serial_ = nullptr;
  uint8_t buf_[32];
  int idx_ = 0;
  uint16_t pm1_ = 0, pm25_ = 0, pm10_ = 0;
  uint32_t lastMs_ = 0;
};
