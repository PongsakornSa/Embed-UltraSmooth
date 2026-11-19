// Sensor node (outdoor) - Smart Classroom
//
// Reads outdoor PM2.5/PM10 (PMS5003), temperature/humidity (SHT31), rain and smoke (MQ-2),
// then sends a SensorPacket to the gateway over ESP-NOW.
//
// Reliability (E4):
//  - every packet waits for an application-level ACK from the gateway
//  - up to MAX_RETRIES re-sends, then the packet goes to a backlog buffer
//  - backlog is flushed (oldest first) as soon as the gateway answers again
//  - if the gateway stays silent, the node hops Wi-Fi channels to find it
//    (the gateway's ESP-NOW channel = the channel of its Wi-Fi hotspot)

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

#include "config.h"
#include "protocol.h"
#include "drivers/pms5003.h"
#include "drivers/sht31.h"

static Pms5003 pms;
static Sht31 sht;
static Preferences prefs;

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t gatewayMac[6];
static bool gatewayKnown = false;
static uint8_t channel = 1;
static int failStreak = 0;

// ACK handshake between the ESP-NOW callback (Wi-Fi task) and loop()
static volatile uint32_t ackSeq = 0;
static volatile bool ackFlag = false;
static char ackNode[NODE_ID_LEN];
static uint8_t ackMac[6];

// Statistics for the PDR / latency metrics (printed on Serial)
static uint32_t statGenerated = 0, statSent = 0, statAcked = 0, statRetries = 0, statLost = 0;
static uint32_t rttSumMs = 0, rttMinMs = UINT32_MAX, rttMaxMs = 0;

// Simple ring buffer of packets that did not get an ACK
static SensorPacket backlog[BACKLOG_SIZE];
static int backHead = 0, backCount = 0;

static uint32_t seqCounter[1 + SIM_EXTRA_NODES] = {0};

// ---------------------------------------------------------------- ESP-NOW
static void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(AckPacket)) return;
  const AckPacket *ack = (const AckPacket *)data;
  if (ack->type != MSG_ACK || ack->version != PROTO_VERSION) return;
  memcpy(ackNode, ack->node_id, NODE_ID_LEN);
  memcpy(ackMac, mac, 6);
  ackSeq = ack->seq;
  ackFlag = true; // set last: loop() reads the other fields after this
}

static void addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0; // 0 = use the current channel
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static void setChannel(uint8_t ch) {
  channel = ch;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  addPeer(BROADCAST);
  if (gatewayKnown) addPeer(gatewayMac);
  Serial.printf("[node] channel -> %u\n", ch);
}

// Send one packet and wait for its ACK. Returns true when acknowledged.
static bool sendOnce(SensorPacket &p) {
  ackFlag = false;
  uint32_t t0 = millis();
  const uint8_t *dest = gatewayKnown ? gatewayMac : BROADCAST;
  if (esp_now_send(dest, (const uint8_t *)&p, sizeof(p)) != ESP_OK) return false;
  statSent++;
  while (millis() - t0 < ACK_TIMEOUT_MS) {
    if (ackFlag && ackSeq == p.seq && strncmp(ackNode, p.node_id, NODE_ID_LEN) == 0) {
      uint32_t rtt = millis() - t0;
      rttSumMs += rtt;
      rttMinMs = min(rttMinMs, rtt);
      rttMaxMs = max(rttMaxMs, rtt);
      if (!gatewayKnown) { // learn the gateway MAC, switch to unicast
        memcpy(gatewayMac, ackMac, 6);
        gatewayKnown = true;
        addPeer(gatewayMac);
      }
      return true;
    }
    delay(2);
  }
  return false;
}

static bool sendReliable(SensorPacket &p) {
  for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    p.retries = attempt;
    if (attempt > 0) statRetries++;
    if (sendOnce(p)) {
      statAcked++;
      digitalWrite(PIN_LED, HIGH);
      delay(20);
      digitalWrite(PIN_LED, LOW);
      return true;
    }
    delay(20 * (attempt + 1)); // small back-off
  }
  return false;
}

// ---------------------------------------------------------------- backlog
static void backlogPush(const SensorPacket &p) {
  int idx = (backHead + backCount) % BACKLOG_SIZE;
  if (backCount == BACKLOG_SIZE) { // full: drop the oldest
    backHead = (backHead + 1) % BACKLOG_SIZE;
    statLost++;
  } else {
    backCount++;
  }
  backlog[idx] = p;
}

// Send everything in the buffer, oldest first, so the gateway sees seq in order.
// Returns true when the buffer is empty afterwards.
static bool backlogFlush() {
  while (backCount > 0) {
    if (!sendReliable(backlog[backHead])) return false; // gateway unreachable
    backHead = (backHead + 1) % BACKLOG_SIZE;
    backCount--;
  }
  return true;
}

// ---------------------------------------------------------------- readings
static void fillHeader(SensorPacket &p, const char *id, int slot) {
  memset(&p, 0, sizeof(p));
  p.type = MSG_SENSOR;
  p.version = PROTO_VERSION;
  strncpy(p.node_id, id, NODE_ID_LEN - 1);
  p.seq = ++seqCounter[slot];
  p.uptime_ms = millis();
}

#if MOCK_SENSORS
// Mock mode (env node_out01_mock): no sensors needed, values keep changing so the
// whole path node -> gateway -> cloud -> dashboard can be shown. Sent as the real
// node id (not SF_SIMULATED) so the gateway uses it for control decisions.
// Cycle = 10 min: PM2.5 crosses the 37.5 limit, rain in minute 7, smoke in minute 9.
static SensorPacket readMockNode() {
  SensorPacket p;
  fillHeader(p, NODE_ID, 0);
  float t = millis() / 1000.0f;
  float minuteInCycle = fmodf(t / 60.0f, 10.0f);
  p.pm25 = 32 + 18 * sinf(t * 2 * PI / 300.0f) + random(-2, 3); // 5 min period, ~12-52
  p.pm10 = p.pm25 * 1.4f + random(0, 4);
  p.temp_c = 31 + 2.5f * sinf(t * 2 * PI / 600.0f) + random(-3, 4) / 10.0f;
  p.rh = 65 + 8 * cosf(t * 2 * PI / 600.0f) + random(-2, 3);
  bool rain = minuteInCycle >= 7 && minuteInCycle < 8;
  p.rain_raw = rain ? 1200 + random(0, 300) : 3900 + random(0, 150);
  p.raining = rain;
  bool smoke = minuteInCycle >= 9;
  p.smoke_raw = smoke ? 2200 + random(0, 300) : 450 + random(0, 80);
  p.smoky = smoke;
  p.sensor_flags = SF_PMS | SF_SHT | SF_RAIN | SF_SMOKE;
  Serial.printf("[MOCK] pm25=%.0f temp=%.1f rain=%d smoke=%d\n", p.pm25, p.temp_c, p.raining, p.smoky);
  return p;
}
#endif

static SensorPacket readRealNode() {
#if MOCK_SENSORS
  return readMockNode();
#endif
  SensorPacket p;
  fillHeader(p, NODE_ID, 0);

  if (millis() - pms.lastFrameMs() < PMS_STALE_MS && pms.lastFrameMs() != 0) {
    p.pm25 = pms.pm25();
    p.pm10 = pms.pm10();
    p.sensor_flags |= SF_PMS;
  } else {
    p.pm25 = p.pm10 = NAN;
  }

  float t, h;
  if (sht.read(t, h)) {
    p.temp_c = t;
    p.rh = h;
    p.sensor_flags |= SF_SHT;
  } else {
    p.temp_c = p.rh = NAN;
  }

  p.rain_raw = analogRead(PIN_RAIN_AO);
  p.raining = p.rain_raw < RAIN_WET_THRESHOLD;
  p.sensor_flags |= SF_RAIN;

  // MQ-2: average 8 samples to smooth ADC noise
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(PIN_SMOKE_AO);
  p.smoke_raw = sum / 8;
  if (millis() > SMOKE_WARMUP_MS) { // heater still warming up -> not valid yet
    p.smoky = p.smoke_raw > SMOKE_THRESHOLD;
    p.sensor_flags |= SF_SMOKE;
  }
  return p;
}

// Simulated node: plausible values with slow drift + noise.
static SensorPacket readSimNode(int slot) {
  char id[NODE_ID_LEN];
  snprintf(id, sizeof(id), "sim-%02d", slot + 1);
  SensorPacket p;
  fillHeader(p, id, slot);
  float phase = millis() / 600000.0f + slot; // ~1 h period
  p.pm25 = 25 + 20 * sinf(phase) + random(-3, 4);
  p.pm10 = p.pm25 * 1.4f;
  p.temp_c = 31 + 2 * sinf(phase * 0.5f) + random(-5, 6) / 10.0f;
  p.rh = 65 + 10 * cosf(phase) + random(-2, 3);
  p.rain_raw = 4000;
  p.raining = 0;
  p.smoke_raw = 400 + random(0, 50);
  p.smoky = 0;
  p.sensor_flags = SF_PMS | SF_SHT | SF_RAIN | SF_SMOKE | SF_SIMULATED;
  return p;
}

static void handlePacket(SensorPacket p) {
  statGenerated++;
  backlogPush(p); // new reading goes behind any older unsent ones
  if (backlogFlush()) {
    failStreak = 0;
    if (prefs.getUChar("ch", 0) != channel) prefs.putUChar("ch", channel); // remember working channel
    return;
  }
  if (++failStreak >= FAILS_BEFORE_HOP) { // gateway may be on another channel
    failStreak = 0;
    gatewayKnown = false;
    setChannel(channel % 13 + 1);
  }
}

// linkPDR     = ACKed transmissions / all transmissions (raw radio quality)
// deliveryPDR = packets finally ACKed / packets generated (after retries+backlog)
static void printStats() {
  float linkPdr = statSent ? 100.0f * statAcked / statSent : 0;
  float delivPdr = statGenerated ? 100.0f * statAcked / statGenerated : 0;
  Serial.printf("[stats] gen=%lu tx=%lu acked=%lu retries=%lu lost=%lu backlog=%d "
                "rtt(ms) avg=%.1f min=%lu max=%lu linkPDR=%.1f%% deliveryPDR=%.1f%%\n",
                statGenerated, statSent, statAcked, statRetries, statLost, backCount,
                statAcked ? (float)rttSumMs / statAcked : 0.0f,
                rttMinMs == UINT32_MAX ? 0 : rttMinMs, rttMaxMs, linkPdr, delivPdr);
}

// ---------------------------------------------------------------- setup/loop
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!sht.begin()) Serial.println("[node] SHT31 not found");
  pms.begin(Serial2, PIN_PMS_RX, PIN_PMS_TX);
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("[node] ESP-NOW init failed, restarting");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onRecv);

  prefs.begin("node", false);
  setChannel(prefs.getUChar("ch", 1));
  Serial.printf("[node] %s ready, MAC %s, sim nodes=%d\n", NODE_ID, WiFi.macAddress().c_str(), SIM_EXTRA_NODES);
}

void loop() {
  pms.poll(); // keep parsing PMS5003 frames between sends

  static uint32_t lastSend = 0, lastStats = 0;
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    SensorPacket p = readRealNode();
    Serial.printf("[node] seq=%lu pm25=%.0f temp=%.1f rh=%.0f rain=%u(%u) smoke=%u(%u)%s\n",
                  p.seq, p.pm25, p.temp_c, p.rh, p.raining, p.rain_raw, p.smoky, p.smoke_raw,
                  (p.sensor_flags & SF_SMOKE) ? "" : " warming-up");
    handlePacket(p);
    for (int s = 1; s <= SIM_EXTRA_NODES; s++) handlePacket(readSimNode(s));
  }
  if (millis() - lastStats >= 60000) {
    lastStats = millis();
    printStats();
  }
  delay(5);
}
