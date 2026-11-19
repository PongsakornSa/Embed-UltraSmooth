// config.h - Sensor node settings (pins, timing). No secrets here.
#pragma once

#ifndef NODE_ID
#define NODE_ID "out-01"
#endif
#ifndef SIM_EXTRA_NODES
#define SIM_EXTRA_NODES 0
#endif

// ---- Pins (ESP32 DevKit v1) ----
#define PIN_I2C_SDA 21     // SHT31
#define PIN_I2C_SCL 22
#define PIN_PMS_RX 16      // ESP32 RX2 <- PMS5003 TX
#define PIN_PMS_TX 17      // ESP32 TX2 -> PMS5003 RX
#define PIN_SMOKE_AO 34    // MQ-2 analog out through 10k/20k divider (5 V -> 3.3 V)
#define PIN_RAIN_AO 35     // rain module analog out (ADC1 only: ADC2 is blocked by radio)
#define PIN_LED 2          // on-board LED: blinks on every ACK

// ---- Timing ----
#define SEND_INTERVAL_MS 10000 // one reading every 10 s
#define ACK_TIMEOUT_MS 150     // wait this long for the gateway ACK
#define MAX_RETRIES 3          // re-send attempts per packet
#define FAILS_BEFORE_HOP 2     // failed packets before trying the next Wi-Fi channel

// ---- Sensors ----
#define RAIN_WET_THRESHOLD 2500 // ADC below this = raining (calibrate with water)
#define PMS_STALE_MS 30000      // PMS frame older than this = sensor fault
#define SMOKE_WARMUP_MS 60000   // MQ-2 heater needs time before readings are valid
#define SMOKE_THRESHOLD 1800    // ADC above this = smoke (calibrate: clean air vs. incense)

// ---- Node-side buffer (packets kept while gateway is unreachable) ----
#define BACKLOG_SIZE 30
