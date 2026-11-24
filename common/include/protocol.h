// protocol.h - Message format shared by sensor node and gateway (ESP-NOW link).
//
// Every packet carries node_id + seq so the gateway can serve many nodes
// (Scalability) and we can measure packet delivery rate (PDR).
// The gateway answers every SensorPacket with an AckPacket (E4 reliable comm).
#pragma once
#include <stdint.h>

#define PROTO_VERSION 2 // v2: added smoke_raw
#define NODE_ID_LEN 8 // e.g. "out-01", null-terminated

enum MsgType : uint8_t {
  MSG_SENSOR = 1, // node -> gateway
  MSG_ACK = 2,    // gateway -> node
};

// sensor_flags bits: 1 = sensor read OK in this cycle
enum SensorFlag : uint8_t {
  SF_PMS = 1 << 0,
  SF_SHT = 1 << 1,
  SF_RAIN = 1 << 2,
  SF_SMOKE = 1 << 3, // MQ-2 warmed up and reading
  SF_GAS_SIMULATED = 1 << 5, // generated gas ADC value; not valid for control
  SF_DHT_SIMULATED = 1 << 6, // temp/RH are generated because DHT read failed
  SF_SIMULATED = 1 << 7, // packet produced by a simulated node
};

#pragma pack(push, 1)
struct SensorPacket {
  uint8_t type;            // MSG_SENSOR
  uint8_t version;         // PROTO_VERSION
  char node_id[NODE_ID_LEN];
  uint32_t seq;            // increments every new reading
  uint32_t uptime_ms;      // node millis() when read
  uint8_t retries;         // how many times this packet was re-sent
  uint8_t sensor_flags;    // SensorFlag bits
  float pm25;              // ug/m3 (outdoor)
  float pm10;              // ug/m3
  float temp_c;            // outdoor temperature
  float rh;                // outdoor relative humidity %
  uint16_t rain_raw;       // ADC 0..4095 (lower = wetter)
  uint8_t raining;         // 1 if rain detected
  uint16_t smoke_raw;      // MQ-2 ADC 0..4095 (higher = more smoke/gas)
  uint8_t smoky;           // 1 if smoke above threshold
};

struct AckPacket {
  uint8_t type;            // MSG_ACK
  uint8_t version;
  char node_id[NODE_ID_LEN];
  uint32_t seq;            // seq being acknowledged
};
#pragma pack(pop)

static_assert(sizeof(SensorPacket) <= 250, "ESP-NOW payload max 250 bytes");
