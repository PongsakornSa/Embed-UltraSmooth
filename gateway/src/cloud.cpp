// cloud.cpp - Wi-Fi, NETPIE (MQTT over TLS, E5), store-and-forward (E4),
// dashboard commands, Telegram alerts (M6) and OTA updates (E6).
#include "app.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <time.h>
#include <stdarg.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h not found - using secrets.example.h placeholders (copy and fill it in)"
#include "secrets.example.h"
#endif

static WiFiClientSecure tlsClient;
static PubSubClient mqtt(tlsClient);

// ------------------------------------------------------------------ alerts
static portMUX_TYPE alertMux = portMUX_INITIALIZER_UNLOCKED;
static struct { char key[16]; uint32_t lastMs; } cooldown[16];

void raiseAlert(const char *key, const char *fmt, ...) {
  uint32_t now = millis();
  bool send = false;
  portENTER_CRITICAL(&alertMux);
  int slot = -1, freeSlot = -1;
  for (int i = 0; i < 16; i++) {
    if (cooldown[i].key[0] == 0) { if (freeSlot < 0) freeSlot = i; continue; }
    if (strncmp(cooldown[i].key, key, 15) == 0) { slot = i; break; }
  }
  if (slot < 0 && freeSlot >= 0) {
    slot = freeSlot;
    strncpy(cooldown[slot].key, key, 15);
    cooldown[slot].lastMs = 0;
  }
  if (slot >= 0 && (cooldown[slot].lastMs == 0 || now - cooldown[slot].lastMs > ALERT_COOLDOWN_MS)) {
    cooldown[slot].lastMs = now;
    send = true;
  }
  portEXIT_CRITICAL(&alertMux);
  if (!send) return;

  AlertMsg m;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(m.text, sizeof(m.text), fmt, ap);
  va_end(ap);
  Serial.printf("[alert] %s\n", m.text);
  xQueueSend(qAlerts, &m, 0); // dropped if the queue is full
}

static bool sendTelegram(const char *text) {
  if (strchr(TELEGRAM_BOT_TOKEN, ':') == nullptr || strstr(TELEGRAM_BOT_TOKEN, "...")) return false;
  WiFiClientSecure tg;
  tg.setInsecure(); // encrypted; Telegram certificate not pinned (see README)
  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN + "/sendMessage";
  if (!http.begin(tg, url)) return false;
  http.addHeader("Content-Type", "application/json");
  JsonDocument doc;
  doc["chat_id"] = TELEGRAM_CHAT_ID;
  doc["text"] = String("[Smart Classroom] ") + text;
  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  return code == 200;
}

// ------------------------------------------------------------------ snapshot
static String buildSnapshot() {
  JsonDocument doc;
  JsonObject d = doc["data"].to<JsonObject>();
  Lock l;
  d["ts"] = (uint32_t)time(nullptr);
  const IndoorState &in = g.in;
  if (!isnan(in.tempC)) d["in_temp"] = serialized(String(in.tempC, 2));
  if (!isnan(in.rh)) d["in_rh"] = serialized(String(in.rh, 1));
  if (in.scdOk) d["co2"] = in.co2;
  if (!isnan(in.lux)) d["lux"] = (int)in.lux;
  d["occupied"] = in.occupied ? 1 : 0;
  d["in_smoke"] = in.smokeRaw;
  d["in_smoke_ok"] = in.smokeOk ? 1 : 0;
  d["in_smoky"] = in.smoky ? 1 : 0;

  const NodeState *out = primaryOutdoorNode();
  if (out) {
    const SensorPacket &p = out->last;
    if (p.sensor_flags & SF_PMS) { d["out_pm25"] = p.pm25; d["out_pm10"] = p.pm10; }
    if (p.sensor_flags & SF_SHT) {
      d["out_temp"] = serialized(String(p.temp_c, 2));
      d["out_rh"] = serialized(String(p.rh, 1));
    }
    d["raining"] = p.raining;
    d["rain_raw"] = p.rain_raw;
    if (p.sensor_flags & SF_SMOKE) {
      d["out_smoke"] = p.smoke_raw;
      d["smoky"] = p.smoky;
    }
  }

  const ActState &a = g.act;
  d["ac_on"] = a.acOn ? 1 : 0;
  d["fan_pct"] = a.fanPct;
  d["ac_src"] = sourceName(a.acSource);
  d["ac_manual"] = millis() < a.acManualUntil ? 1 : 0;
  d["window_open"] = a.windowTarget == WindowTarget::OPEN ? 1 : 0;
  d["window_angle"] = a.windowAngle;
  d["window_src"] = sourceName(a.windowSource);
  d["window_fault"] = a.windowFault ? 1 : 0;
  d["estop"] = a.estop ? 1 : 0;
  d["ai_prob"] = serialized(String(g.ai.prob, 3));
  d["ai_valid"] = g.ai.valid ? 1 : 0;
  d["ac_presses"] = g.stats.acManualOnPresses;

  int online = 0;
  JsonObject nodes = d["nodes"].to<JsonObject>();
  for (int i = 0; i < g.nodeCount; i++) {
    const NodeState &n = g.nodes[i];
    uint32_t age = (millis() - n.lastSeenMs) / 1000;
    if (age * 1000 < NODE_OFFLINE_MS) online++;
    JsonObject o = nodes[n.id].to<JsonObject>();
    o["age_s"] = age;
    o["rx"] = n.received;
    o["gaps"] = n.gaps;
    o["dup"] = n.duplicates;
    o["sim"] = n.simulated ? 1 : 0;
    if (n.simulated) { o["pm25"] = n.last.pm25; o["temp"] = n.last.temp_c; }
  }
  d["nodes_online"] = online;
  d["uptime_s"] = millis() / 1000;
  d["boots"] = g.stats.bootCount;
  d["heap"] = ESP.getFreeHeap();
  d["buffered"] = g.stats.buffered;

  String s;
  serializeJson(doc, s);
  return s;
}

// ------------------------------------------------------------------ store-and-forward
static void bufferAppend(const String &line) {
  File f = LittleFS.open(BUFFER_FILE, FILE_APPEND);
  if (!f) return;
  if (f.size() + line.length() + 1 > BUFFER_MAX_BYTES) { // full: keep the oldest data, drop new
    f.close();
    return;
  }
  f.println(line);
  f.close();
  Lock l;
  g.stats.buffered++;
}

// Publish buffered records (oldest first). Returns true when the buffer is empty.
static bool bufferFlush() {
  if (!LittleFS.exists(BUFFER_FILE)) return true;
  File f = LittleFS.open(BUFFER_FILE, FILE_READ);
  if (!f) return true;
  int sent = 0;
  String failed; // line that could not be published (kept for next time)
  while (f.available() && sent < BUFFER_FLUSH_PER_LOOP) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (!mqtt.publish(TOPIC_SHADOW, line.c_str())) { failed = line + "\n"; break; }
    sent++;
    mqtt.loop();
  }
  String rest = failed + f.readString(); // everything not yet published
  f.close();
  bool empty = rest.isEmpty();
  if (empty) {
    LittleFS.remove(BUFFER_FILE);
  } else {
    File w = LittleFS.open(BUFFER_FILE, FILE_WRITE);
    if (w) { w.print(rest); w.close(); }
  }
  Lock l;
  g.stats.flushed += sent;
  if (empty) g.stats.buffered = 0;
  return empty;
}

// ------------------------------------------------------------------ commands
// Dashboard publishes JSON to @msg/cmd, e.g. {"ac":"on"} {"window":"auto"} {"estop":"reset"}
static void onMqtt(char *topic, byte *payload, unsigned int len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;
  auto mode = [](const char *s) -> int8_t {
    if (!s) return -1;
    if (!strcmp(s, "off") || !strcmp(s, "close")) return 0;
    if (!strcmp(s, "on") || !strcmp(s, "open")) return 1;
    if (!strcmp(s, "auto")) return 2;
    return -1;
  };
  int8_t v;
  if ((v = mode(doc["ac"])) >= 0) { Event e{EvType::CMD_AC, v}; xQueueSend(qEvents, &e, 0); }
  if ((v = mode(doc["window"])) >= 0) { Event e{EvType::CMD_WINDOW, v}; xQueueSend(qEvents, &e, 0); }
  if (doc["estop"] == "reset") { Event e{EvType::CMD_ESTOP_RESET, 0}; xQueueSend(qEvents, &e, 0); }
  Serial.printf("[cloud] cmd %.*s\n", len, (const char *)payload);
}

static bool mqttConnect() {
  if (strlen(NETPIE_ROOT_CA) > 0) tlsClient.setCACert(NETPIE_ROOT_CA); // verified TLS
  else tlsClient.setInsecure();                                       // encrypted only
  mqtt.setServer(MQTT_HOST, MQTT_PORT_TLS);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(1536);
  mqtt.setKeepAlive(30);
  if (!mqtt.connect(NETPIE_CLIENT_ID, NETPIE_TOKEN, NETPIE_SECRET)) {
    Serial.printf("[cloud] MQTT connect failed, state=%d\n", mqtt.state());
    return false;
  }
  mqtt.subscribe(TOPIC_CMD);
  Serial.println("[cloud] MQTT connected (TLS)");
  return true;
}

static void otaBegin() {
  ArduinoOTA.setHostname(GW_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([] {
    Lock l;
    g.act.estop = true; // freeze actuators during the update
    fanSet(0);
  });
  ArduinoOTA.onEnd([] { Serial.println("[ota] done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", e); });
  ArduinoOTA.begin();
}

// ------------------------------------------------------------------ task
void cloudTask(void *) {
  pinMode(PIN_LED_STATUS, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(GW_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false); // keep the radio awake so ESP-NOW packets are not missed
  espnowBegin();        // works even before Wi-Fi connects (nodes hop channels)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");

  bool otaStarted = false, bootAlertSent = false;
  uint32_t lastPublish = 0, lastMqttTry = 0, wifiLostSince = 0;

  for (;;) {
    uint32_t now = millis();
    bool wifiUp = WiFi.status() == WL_CONNECTED;

    if (wifiUp) {
      wifiLostSince = 0;
      if (!otaStarted) { otaBegin(); otaStarted = true; Serial.printf("[cloud] Wi-Fi %s ch %d\n", WiFi.localIP().toString().c_str(), WiFi.channel()); }
      ArduinoOTA.handle();
      if (!mqtt.connected() && now - lastMqttTry > 5000) {
        lastMqttTry = now;
        mqttConnect();
      }
      if (mqtt.connected()) {
        mqtt.loop();
        bufferFlush();
      }
    } else {
      if (wifiLostSince == 0) wifiLostSince = now;
      if (now - wifiLostSince > 60000) { WiFi.reconnect(); wifiLostSince = now; }
    }

    bool cloudOk = wifiUp && mqtt.connected();
    { Lock l; g.cloudOk = cloudOk; }

    if (now - lastPublish >= PUBLISH_PERIOD_MS) {
      lastPublish = now;
      String snap = buildSnapshot();
      bool ok = cloudOk && mqtt.publish(TOPIC_SHADOW, snap.c_str());
      {
        Lock l;
        if (ok) g.stats.published++;
        else g.stats.publishFailed++;
      }
      if (!ok) bufferAppend(snap); // store-and-forward: keep it until the cloud is back
    }

    // Alerts: only dequeue while online so nothing is lost during an outage
    if (wifiUp) {
      if (!bootAlertSent) {
        bootAlertSent = true;
        char msg[64];
        snprintf(msg, sizeof(msg), "Gateway online (boot #%lu)", (unsigned long)g.stats.bootCount);
        sendTelegram(msg);
      }
      AlertMsg m;
      if (xQueueReceive(qAlerts, &m, 0) == pdTRUE) {
        sendTelegram(m.text);
        if (mqtt.connected()) mqtt.publish(TOPIC_ALERT, m.text);
      }
    }

    digitalWrite(PIN_LED_STATUS, cloudOk ? HIGH : ((now / 500) % 2));
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
