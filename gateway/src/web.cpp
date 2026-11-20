// The browser reads cached NETPIE REST responses, never local ESP-NOW readings.
#include "app.h"
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "web_page.h"
#include "web_metrics.h"
#include "gas_fan_model.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

static WebServer server(80);
static String shadowCache;
static bool readOk = false;
static uint32_t readAtMs = 0;
static int httpStatus = 0;
static double mq135R0Factor = 0;
static uint16_t mq135R0Raw = 0;
static bool mq135R0Ready = false;
static bool mq135R0Loaded = false;
static bool gasFanOutputOn = false;
static uint32_t gasFanLastSwitchMs = 0;
static uint8_t gasFanCandidateCount = 0;
static bool gasFanCandidateOn = false;
static constexpr double MQ135_SENSOR_SUPPLY_V = 5.0;
static constexpr double MQ135_ADC_VREF = 3.3;
static constexpr double MQ135_ADC_DIVIDER_GAIN = 1.5; // 10k/20k divider documented on Node
// Robust median of the 260 baseline ADC samples provided for this device.
static constexpr uint16_t MQ135_BASELINE_RAW = 436;
static void loadMQ135Calibration();

static double mq135ResistanceFactor(uint16_t raw) {
  const double vOut = (double)raw * MQ135_ADC_VREF / 4095.0 * MQ135_ADC_DIVIDER_GAIN;
  return vOut > 0 && vOut < MQ135_SENSOR_SUPPLY_V
      ? MQ135_SENSOR_SUPPLY_V / vOut - 1.0 : 0;
}

static float predictGasFan(float temperature, float humidity, float gasRsR0) {
  const float values[3] = {temperature, humidity, gasRsR0};
  float logit = GAS_FAN_WEIGHTS[0];
  for (int i = 0; i < 3; ++i)
    logit += GAS_FAN_WEIGHTS[i + 1] * ((values[i] - GAS_FAN_MEAN[i]) / GAS_FAN_STD[i]);
  logit = constrain(logit, -30.0f, 30.0f);
  return 1.0f / (1.0f + expf(-logit));
}

static bool readGasFanInputs(float &temperature, float &humidity, float &gasRsR0) {
  loadMQ135Calibration();
  String cached;
  bool freshShadow, r0Ready;
  double r0Factor;
  {
    Lock l;
    cached = shadowCache;
    freshShadow = readOk && millis() - readAtMs < 15000;
    r0Ready = mq135R0Ready;
    r0Factor = mq135R0Factor;
  }
  if (!freshShadow || !r0Ready || cached.isEmpty()) return false;
  JsonDocument shadow;
  if (deserializeJson(shadow, cached)) return false;
  const time_t now = time(nullptr);
  const uint32_t ts = shadow["ts"] | 0U;
  const uint32_t nodeAge = shadow["mq135_age_s"] | UINT32_MAX;
  const uint32_t sampleTs = shadow["mq135_ts"] | ts;
  const uint16_t raw = shadow["mq135_raw"] | 0U;
  if (now < 1600000000 || ts < 1600000000U || now < ts || now - ts > 15 ||
      sampleTs < 1600000000U || now < sampleTs || now - sampleTs > 45 || nodeAge > 45 ||
      (shadow["mq135_online"] | 0) != 1 || !(shadow["mq135_warmed_up"] | false) ||
      raw < 20 || raw > 4000) return false;
  JsonVariantConst temp = shadow["mq135_temp"];
  JsonVariantConst rh = shadow["mq135_rh"];
  if (!temp.is<double>() || !rh.is<double>()) return false;
  temperature = temp.as<float>();
  humidity = rh.as<float>();
  const double vOut = (double)raw * MQ135_ADC_VREF / 4095.0 * MQ135_ADC_DIVIDER_GAIN;
  if (!isfinite(temperature) || !isfinite(humidity) || vOut <= 0 ||
      vOut >= MQ135_SENSOR_SUPPLY_V || !isfinite(r0Factor) || r0Factor <= 0) return false;
  gasRsR0 = (MQ135_SENSOR_SUPPLY_V / vOut - 1.0) / r0Factor;
  return isfinite(gasRsR0) && gasRsR0 >= 0;
}

static void updateGasFanOutput() {
  static uint32_t lastEvaluationMs = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastEvaluationMs < 2000) return;
  lastEvaluationMs = nowMs;
  float temperature, humidity, gasRsR0;
  const bool emergencyStop = digitalRead(PIN_BTN_ESTOP) == LOW;
  const bool inputsOk = !emergencyStop && readGasFanInputs(temperature, humidity, gasRsR0);
  if (!inputsOk) {
    if (gasFanOutputOn) {
      fanSet(0);
      Lock l;
      gasFanOutputOn = false;
      gasFanLastSwitchMs = nowMs;
    }
    gasFanCandidateCount = 0;
    return;
  }
  const float probability = predictGasFan(temperature, humidity, gasRsR0);
  const bool shouldOpen = probability >= GAS_FAN_THRESHOLD;
  if (shouldOpen == gasFanOutputOn) {
    gasFanCandidateCount = 0;
    return;
  }
  if (shouldOpen != gasFanCandidateOn) {
    gasFanCandidateOn = shouldOpen;
    gasFanCandidateCount = 1;
    return;
  }
  if (gasFanCandidateCount < 3) ++gasFanCandidateCount;
  if (gasFanCandidateCount >= 3 && nowMs - gasFanLastSwitchMs >= 30000) {
    fanSet(shouldOpen ? 100 : 0);
    Lock l;
    gasFanOutputOn = shouldOpen;
    gasFanLastSwitchMs = nowMs;
    gasFanCandidateCount = 0;
    Serial.printf("[gas-fan] %s p=%.3f temp=%.2f rh=%.1f rs/r0=%.3f\n",
                  shouldOpen ? "ON" : "OFF", probability, temperature, humidity, gasRsR0);
  }
}

static void loadMQ135Calibration() {
  if (mq135R0Loaded) return;
  Preferences prefs;
  double factor = 0;
  uint16_t raw = 0;
  if (prefs.begin("mq135", true)) {
    factor = prefs.getDouble("r0_f_v2", 0);
    raw = prefs.getUShort("r0_a_v2", 0);
    prefs.end();
  }
  if (!isfinite(factor) || factor <= 0 || raw == 0) {
    raw = MQ135_BASELINE_RAW;
    factor = mq135ResistanceFactor(raw);
    Preferences initPrefs;
    if (factor > 0 && initPrefs.begin("mq135", false)) {
      initPrefs.putDouble("r0_f_v2", factor);
      initPrefs.putUShort("r0_a_v2", raw);
      initPrefs.end();
    }
  }
  Lock l;
  mq135R0Factor = factor;
  mq135R0Raw = raw;
  mq135R0Ready = isfinite(factor) && factor > 0 && raw > 0;
  mq135R0Loaded = true;
}

static void calibrateMQ135Response() {
  String cached;
  bool freshShadow;
  {
    Lock l;
    cached = shadowCache;
    freshShadow = readOk && millis() - readAtMs < 15000;
  }
  JsonDocument shadow;
  if (!freshShadow || cached.isEmpty() || deserializeJson(shadow, cached)) {
    server.send(503, "application/json", "{\"error\":\"NETPIE Shadow ยังไม่พร้อม\"}");
    return;
  }
  time_t now = time(nullptr);
  uint32_t shadowTs = shadow["ts"] | 0U;
  uint32_t nodeAge = shadow["mq135_age_s"] | UINT32_MAX;
  uint16_t raw = shadow["mq135_raw"] | 0U;
  if (now < 1600000000 || shadowTs < 1600000000U || now - shadowTs > 15 ||
      (shadow["mq135_online"] | 0) != 1 || !(shadow["mq135_warmed_up"] | false) ||
      nodeAge > 10 || raw < 20 || raw > 4000) {
    server.send(409, "application/json",
      "{\"error\":\"ต้องต่อ Node รอเซนเซอร์อุ่น วางในอากาศสะอาด และมีค่า ADC สดก่อนตั้ง R0\"}");
    return;
  }
  const double factor = mq135ResistanceFactor(raw);
  if (!isfinite(factor) || factor <= 0) {
    server.send(422, "application/json", "{\"error\":\"ค่าแรงดันเซนเซอร์ใช้ตั้ง R0 ไม่ได้\"}");
    return;
  }
  Preferences prefs;
  if (!prefs.begin("mq135", false)) {
    server.send(500, "application/json", "{\"error\":\"บันทึกค่า R0 ไม่สำเร็จ\"}");
    return;
  }
  size_t wroteFactor = prefs.putDouble("r0_f_v2", factor);
  size_t wroteRaw = prefs.putUShort("r0_a_v2", raw);
  prefs.end();
  if (!wroteFactor || !wroteRaw) {
    server.send(500, "application/json", "{\"error\":\"บันทึกค่า R0 ไม่สำเร็จ\"}");
    return;
  }
  {
    Lock l;
    mq135R0Factor = factor;
    mq135R0Raw = raw;
    mq135R0Ready = true;
  }
  JsonDocument result;
  result["ok"] = true;
  result["r0_adc"] = raw;
  result["response_at_calibration_rs_r0"] = 1.0;
  String body;
  serializeJson(result, body);
  server.send(200, "application/json; charset=utf-8", body);
}

// Called only by webDataTask: blocking HTTPS never stalls the HTTP server loop.
void webCloudRefresh() {
  static uint32_t lastTry = 0;
  uint32_t now = millis();
  if (now - lastTry < WEB_SHADOW_POLL_MS) return;
  lastTry = now;
  int status = 0;
  bool ok = false;
  String body;
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // same encrypted-only TLS policy as the existing cloud client
    client.setHandshakeTimeout(5);
    HTTPClient http;
    http.useHTTP10(true); // JSON parser reads the body without chunked framing
    http.setConnectTimeout(3000);
    http.setTimeout(3000);
    if (http.begin(client, "https://api.netpie.io/v2/device/shadow/data")) {
      http.addHeader("Authorization", String("Device ") + NETPIE_CLIENT_ID + ":" + NETPIE_TOKEN);
      status = http.GET();
      if (status == 200) {
        int size = http.getSize();
        if (size <= 16384) {
          JsonDocument doc;
          // Parse directly from the response stream: avoid copying an unbounded body.
          if (!deserializeJson(doc, http.getStream()) && doc["data"].is<JsonObject>()) {
            serializeJson(doc["data"], body);
            ok = true;
          }
        }
      }
      http.end();
    }
  }
  {
    Lock l;
    readOk = ok;
    httpStatus = status;
    if (ok) { shadowCache = body; readAtMs = millis(); }
  }
  Serial.printf("[web] NETPIE shadow %s HTTP=%d\n", ok ? "read OK" : "unavailable", status);
}

struct HistoryCache {
  String body;
  uint32_t storedAt = 0;
  bool ok = false;
  uint64_t startMs = 0, endMs = 0;
};
static const float historyHours[] = {5.0f / 60.0f, 0.25f, 1, 72, 168, 360, 720};
static const char *historyRangeValues[] = {"0.08333333333333333", "0.25", "1", "72", "168", "360", "720"};
static constexpr size_t HISTORY_RANGE_COUNT = sizeof(historyHours) / sizeof(historyHours[0]);
static constexpr size_t HISTORY_CACHE_COUNT = WEB_METRIC_COUNT * HISTORY_RANGE_COUNT;
static HistoryCache historyCache[HISTORY_CACHE_COUNT];
static uint64_t historyPending = 0;
static_assert(HISTORY_CACHE_COUNT <= 64, "history pending mask capacity");
struct LatestFeedValue { double value = 0; uint64_t timestampMs = 0; bool valid = false; };
static LatestFeedValue latestFeedValues[WEB_METRIC_COUNT];

// Runs in webDataTask, serially with shadow reads, never in HTTP handlers.
void webHistoryRefresh() {
  int slot = -1;
  uint64_t startMs = 0, endMs = 0;
  {
    Lock l;
    static size_t nextSlot = 0;
    for (size_t offset = 0; offset < HISTORY_CACHE_COUNT; ++offset) {
      size_t i = (nextSlot + offset) % HISTORY_CACHE_COUNT;
      if (historyPending & (1ULL << i)) {
        slot = i;
        startMs = historyCache[i].startMs;
        endMs = historyCache[i].endMs;
        nextSlot = (i + 1) % HISTORY_CACHE_COUNT;
        break;
      }
    }
  }
  if (slot < 0) return;
  int range = slot % HISTORY_RANGE_COUNT;
  const WebMetric &spec = WEB_METRICS[slot / HISTORY_RANGE_COUNT];
  JsonDocument result;
  result["metric"] = spec.id;
  result["field"] = spec.field;
  result["source"] = "netpie-feed";
  result["hours"] = historyHours[range];
  result["aggregation"] = "raw";
  result["start_ms"] = startMs;
  result["end_ms"] = endMs;
  result["next_after"] = nullptr;
  result["state"] = "error";
  JsonArray points = result["points"].to<JsonArray>();
  LatestFeedValue newest;
  int status = 0;
  bool ok = false;
  if (WiFi.status() == WL_CONNECTED) {
    JsonDocument query;
    query["start_absolute"] = startMs;
    query["end_absolute"] = endMs;
    JsonObject metric = query["metrics"].add<JsonObject>();
    metric["name"] = NETPIE_CLIENT_ID;
    metric["tags"]["attr"].add(spec.field);
    JsonObject group = metric["group_by"].add<JsonObject>();
    group["name"] = "tag";
    group["tags"].add("attr");
    metric["limit"] = 300; // raw samples, paginated to bound ESP32 memory
    metric["order"] = "asc";
    String request;
    serializeJson(query, request);
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(5);
    HTTPClient http;
    http.useHTTP10(true);
    http.setConnectTimeout(3000);
    http.setTimeout(3000);
    if (http.begin(client, "https://ds.netpie.io/feed/api/v1/datapoints/query")) {
      http.addHeader("Authorization", String("Device ") + NETPIE_CLIENT_ID + ":" + NETPIE_TOKEN);
      http.addHeader("Content-Type", "application/json");
      status = http.POST(request);
      if (status == 200) {
        JsonDocument feed, filter;
        filter["queries"][0]["results"][0]["values"] = true;
        if (!deserializeJson(feed, http.getStream(), DeserializationOption::Filter(filter)) &&
            feed["queries"][0]["results"].is<JsonArray>()) {
          JsonArrayConst values = feed["queries"][0]["results"][0]["values"];
          uint64_t lastTimestamp = 0;
          for (JsonArrayConst pair : values) {
            if (points.size() >= 300) break;
            if (pair.size() != 2 || !pair[0].is<double>() || !pair[1].is<double>()) continue;
            double timestamp = pair[0].as<double>(), value = pair[1].as<double>();
            if (isfinite(timestamp) && timestamp >= startMs && timestamp <= endMs) lastTimestamp = (uint64_t)timestamp;
            if (!isfinite(timestamp) || !isfinite(value) || value < spec.min || value > spec.max) continue;
            JsonArray point = points.add<JsonArray>();
            point.add(timestamp);
            point.add(value);
            if (!newest.valid || (uint64_t)timestamp > newest.timestampMs) {
              newest.value = value;
              newest.timestampMs = (uint64_t)timestamp;
              newest.valid = true;
            }
          }
          if (values.size() >= 300 && lastTimestamp >= startMs && lastTimestamp < endMs) result["next_after"] = lastTimestamp + 1;
          result["state"] = "ready";
          ok = true;
        }
      }
      http.end();
    }
  }
  result["http"] = status;
  String body;
  serializeJson(result, body);
  {
    Lock l;
    if (historyCache[slot].startMs != startMs || historyCache[slot].endMs != endMs) return;
    historyPending &= ~(1ULL << slot);
    historyCache[slot].body = body;
    historyCache[slot].storedAt = millis();
    historyCache[slot].ok = ok;
    size_t metricIndex = (size_t)(slot / HISTORY_RANGE_COUNT);
    if (ok && newest.valid && (!latestFeedValues[metricIndex].valid ||
        newest.timestampMs >= latestFeedValues[metricIndex].timestampMs))
      latestFeedValues[metricIndex] = newest;
    // Bound total history strings as more sensor/range combinations are selected.
    for (;;) {
      size_t bytes = 0;
      int oldest = -1;
      for (size_t i = 0; i < HISTORY_CACHE_COUNT; ++i) {
        bytes += historyCache[i].body.length();
        if ((int)i != slot && !historyCache[i].body.isEmpty() &&
            (oldest < 0 || millis() - historyCache[i].storedAt > millis() - historyCache[oldest].storedAt)) oldest = i;
      }
      if (bytes <= 24000 || oldest < 0) break;
      historyCache[oldest].body = "";
    }
  }
  Serial.printf("[web] NETPIE Feed %s %s h HTTP=%d points=%u\n", spec.id, historyRangeValues[range], status, (unsigned)points.size());
}

void webDataTask(void *) {
  for (;;) {
    webCloudRefresh();
#if GATEWAY_GAS_FAN_CONTROL
    updateGasFanOutput();
#endif
    webHistoryRefresh();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void historyResponse() {
  if (time(nullptr) < 1600000000) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(202, "application/json", "{\"state\":\"loading\"}");
    return; // wait for NTP before querying absolute history timestamps
  }
  String hours = server.arg("hours");
  int range = -1;
  for (size_t i = 0; i < HISTORY_RANGE_COUNT; ++i)
    if (hours == historyRangeValues[i]) range = i;
  if (range < 0) {
    server.send(400, "application/json", "{\"error\":\"hours must be 0.08333333333333333, 0.25, 1, 72, 168, 360 or 720\"}");
    return;
  }
  String metricId = server.hasArg("metric") ? server.arg("metric") : "mq135";
  int metricIndex = -1;
  for (size_t i = 0; i < WEB_METRIC_COUNT; ++i)
    if (metricId == WEB_METRICS[i].id) metricIndex = i;
  if (metricIndex < 0) {
    server.send(400, "application/json", "{\"error\":\"unknown metric\"}");
    return;
  }
  int slot = metricIndex * HISTORY_RANGE_COUNT + range;
  uint64_t nowMs = (uint64_t)time(nullptr) * 1000;
  uint64_t rangeMs = (uint64_t)(historyHours[range] * 3600000.0f);
  uint64_t startMs = 0, endMs = 0;
  auto parseTime = [](const String &text, uint64_t &value) {
    if (text.length() < 10 || text.length() > 16) return false;
    value = 0;
    for (size_t i = 0; i < text.length(); ++i) {
      if (text[i] < '0' || text[i] > '9') return false;
      value = value * 10 + text[i] - '0';
    }
    return true;
  };
  bool absolute = server.hasArg("after") || server.hasArg("until");
  if (absolute && (!parseTime(server.arg("after"), startMs) || !parseTime(server.arg("until"), endMs) ||
                   startMs >= endMs || endMs - startMs > rangeMs + 1000 || endMs > nowMs + 300000)) {
    server.send(400, "application/json", "{\"error\":\"invalid time window\"}");
    return;
  }
  String body;
  {
    Lock l;
    HistoryCache &cache = historyCache[slot];
    uint32_t ttl = cache.ok ? 60000 : 15000;
    if (!absolute) {
      if (cache.endMs && ((!cache.body.isEmpty() && millis() - cache.storedAt < ttl) || (historyPending & (1ULL << slot)))) {
        startMs = cache.startMs; endMs = cache.endMs;
      } else { endMs = nowMs; startMs = endMs - rangeMs; }
    }
    bool matches = cache.startMs == startMs && cache.endMs == endMs;
    if (matches && !cache.body.isEmpty() && millis() - cache.storedAt < ttl) body = cache.body;
    else {
      cache.startMs = startMs; cache.endMs = endMs; cache.body = "";
      historyPending |= 1ULL << slot;
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  if (body.isEmpty()) server.send(202, "application/json", "{\"state\":\"loading\"}");
  else server.send(200, "application/json; charset=utf-8", body);
}

static void statusResponse() {
  String cached;
  bool netpieOk, cloudOk;
  int status;
  bool r0Ready;
  bool fanOutputOn;
  double r0Factor;
  uint16_t r0Raw;
  uint32_t fetchedAt, published, failed, buffered;
  LatestFeedValue latestValues[WEB_METRIC_COUNT] = {};
  {
    Lock l;
    cached = shadowCache;
    netpieOk = readOk;
    fetchedAt = readAtMs;
    status = httpStatus;
    cloudOk = g.cloudOk;
    published = g.stats.published;
    failed = g.stats.publishFailed;
    buffered = g.stats.buffered;
    r0Ready = mq135R0Ready;
    fanOutputOn = gasFanOutputOn;
    r0Factor = mq135R0Factor;
    r0Raw = mq135R0Raw;
    for (size_t i = 0; i < WEB_METRIC_COUNT; ++i) latestValues[i] = latestFeedValues[i];
  }
  netpieOk = netpieOk && millis() - fetchedAt < 15000;
  JsonDocument shadow;
  if (!cached.isEmpty()) deserializeJson(shadow, cached);
  JsonDocument d;
  d["source"] = "netpie";
  d["netpie_connected"] = netpieOk;
  d["netpie_http"] = status;
  d["node_id"] = shadow["mq135_node"] | MQ135_NODE_ID;
  d["node_online"] = false;
  d["mq135_raw"] = nullptr;
  d["mq135_r0_ready"] = r0Ready;
  d["mq135_r0_factor"] = r0Ready ? r0Factor : 0;
  d["mq135_r0_adc"] = r0Ready ? r0Raw : 0;
  d["last_mq135_raw"] = shadow["mq135_raw"];
  d["sample_age_s"] = nullptr;
  d["shadow_age_s"] = nullptr;
  d["sequence"] = shadow["mq135_seq"];
  d["sample_id"] = shadow["mq135_sample_id"];
  d["warmed_up"] = shadow["mq135_warmed_up"] | false;
  d["temperature"] = nullptr;
  d["humidity"] = nullptr;
  d["gas_fan_model_ready"] = false;
  d["gas_fan_probability"] = nullptr;
  d["gas_fan_should_open"] = nullptr;
  d["gas_fan_threshold"] = GAS_FAN_THRESHOLD;
  d["gas_fan_output_on"] = fanOutputOn;
  d["pm25"] = nullptr;
  d["pm10"] = nullptr;
  uint32_t ts = shadow["ts"] | 0U;
  time_t now = time(nullptr);
  uint32_t shadowAge = UINT32_MAX, mqSampleAge = UINT32_MAX;
  uint32_t mqAgeAtSnapshot = 0;
  bool shadowFresh = false, mqSampleAgeKnown = false;
  if (ts > 1600000000U && now > 1600000000 && (int64_t)now - ts >= -5) {
    shadowAge = now > ts ? (uint32_t)(now - ts) : 0;
    d["shadow_age_s"] = shadowAge;
    shadowFresh = shadowAge < 45;
    if (shadow["mq135_ts"].is<uint32_t>()) {
      uint32_t sampleTs = shadow["mq135_ts"].as<uint32_t>();
      if ((int64_t)now - sampleTs >= -5) {
        mqSampleAge = now > sampleTs ? (uint32_t)(now - sampleTs) : 0;
        mqSampleAgeKnown = true;
      }
    } else if (shadow["mq135_age_s"].is<uint32_t>()) {
      mqAgeAtSnapshot = shadow["mq135_age_s"].as<uint32_t>();
      mqSampleAge = shadowAge + mqAgeAtSnapshot;
      mqSampleAgeKnown = true;
    }
  }
  bool sourceMatches = strcmp(shadow["mq135_node"] | "", MQ135_NODE_ID) == 0;
  JsonVariantConst mqShadowValue = shadow["mq135_raw"];
  double mqShadowNumber = 0;
  bool mqShadowValid = sourceMatches && mqShadowValue.is<double>();
  if (mqShadowValid) {
    mqShadowNumber = mqShadowValue.as<double>();
    mqShadowValid = isfinite(mqShadowNumber) && mqShadowNumber >= 0 && mqShadowNumber <= 4095;
  }
  bool nodeFresh = netpieOk && shadowFresh && sourceMatches &&
                   (shadow["mq135_online"] | 0) == 1 && mqShadowValid &&
                   mqSampleAgeKnown && mqSampleAge < 45;
  d["node_online"] = nodeFresh;
  if (nodeFresh) {
    d["temperature"] = shadow["mq135_temp"];
    d["humidity"] = shadow["mq135_rh"];
    d["pm25"] = shadow["mq135_pm25"];
    d["pm10"] = shadow["mq135_pm10"];
  }

  JsonObject measurements = d["measurements"].to<JsonObject>();
  JsonObject ages = d["measurement_age_s"].to<JsonObject>();
  uint64_t nowMs = now > 1600000000 ? (uint64_t)now * 1000ULL : 0;
  for (size_t i = 0; i < WEB_METRIC_COUNT; ++i) {
    const WebMetric &metric = WEB_METRICS[i];
    measurements[metric.id] = nullptr;
    ages[metric.id] = nullptr;
    bool isMq135 = strcmp(metric.id, "mq135") == 0;
    bool hasValue = false;
    double value = 0;
    uint64_t sampleMs = 0;
    if (isMq135 && mqShadowValid) {
      hasValue = true;
      value = mqShadowNumber;
      if (shadow["mq135_ts"].is<uint32_t>()) {
        sampleMs = (uint64_t)shadow["mq135_ts"].as<uint32_t>() * 1000ULL;
      } else if (ts > 1600000000U && shadow["mq135_age_s"].is<uint32_t>() &&
                 ts >= mqAgeAtSnapshot) {
        sampleMs = (uint64_t)(ts - mqAgeAtSnapshot) * 1000ULL;
      }
    } else if (!isMq135) {
      JsonVariantConst shadowValue = shadow[metric.field];
      if (shadowValue.is<double>()) {
        double candidate = shadowValue.as<double>();
        if (isfinite(candidate) && candidate >= metric.min && candidate <= metric.max) {
          hasValue = true;
          value = candidate;
          String timestampField = String(metric.field) + "_ts";
          uint32_t sampleTs = shadow[timestampField] | 0U;
          if (sampleTs > 1600000000U) sampleMs = (uint64_t)sampleTs * 1000ULL;
          else if (strcmp(metric.id, "humidity") == 0 && ts > 1600000000U &&
                   shadow["mq135_age_s"].is<uint32_t>() && ts >= mqAgeAtSnapshot)
            sampleMs = (uint64_t)(ts - mqAgeAtSnapshot) * 1000ULL;
        }
      }
    }
    const LatestFeedValue &feedValue = latestValues[i];
    if (feedValue.valid && (!hasValue || !sampleMs || feedValue.timestampMs >= sampleMs)) {
      hasValue = true;
      value = feedValue.value;
      sampleMs = feedValue.timestampMs;
    }
    if (hasValue) {
      measurements[metric.id] = value;
      if (nowMs && sampleMs && sampleMs <= nowMs + 5000ULL) {
        uint64_t age = nowMs > sampleMs ? (nowMs - sampleMs) / 1000ULL : 0;
        ages[metric.id] = age > UINT32_MAX ? UINT32_MAX : (uint32_t)age;
      }
    }
  }
  d["mq135_raw"] = measurements["mq135"];
  d["sample_age_s"] = ages["mq135"];
  d["temperature"] = measurements["temperature"];
  d["humidity"] = measurements["humidity"];
  JsonVariantConst modelTemp = measurements["temperature"];
  JsonVariantConst modelRh = measurements["humidity"];
  JsonVariantConst modelRaw = measurements["mq135"];
  uint32_t tempAge = ages["temperature"] | UINT32_MAX;
  uint32_t rhAge = ages["humidity"] | UINT32_MAX;
  uint32_t gasAge = ages["mq135"] | UINT32_MAX;
  if (GAS_FAN_MODEL_READY && r0Ready && modelTemp.is<double>() && modelRh.is<double>() &&
      modelRaw.is<double>() && tempAge <= 45 && rhAge <= 45 && gasAge <= 45) {
    const double raw = modelRaw.as<double>();
    const double vOut = raw * MQ135_ADC_VREF / 4095.0 * MQ135_ADC_DIVIDER_GAIN;
    const double gasRsR0 = vOut > 0 && vOut < MQ135_SENSOR_SUPPLY_V
        ? (MQ135_SENSOR_SUPPLY_V / vOut - 1.0) / r0Factor : NAN;
    const float temp = modelTemp.as<float>();
    const float rh = modelRh.as<float>();
    if (isfinite(temp) && isfinite(rh) && isfinite(gasRsR0) && gasRsR0 >= 0) {
      const float probability = predictGasFan(temp, rh, gasRsR0);
      d["gas_fan_model_ready"] = true;
      d["gas_fan_probability"] = probability;
      d["gas_fan_should_open"] = probability >= GAS_FAN_THRESHOLD;
      d["gas_fan_temperature_c"] = temp;
      d["gas_fan_humidity_rh"] = rh;
      d["gas_fan_rs_r0"] = gasRsR0;
    }
  }
  JsonVariantConst node = shadow["nodes"][MQ135_NODE_ID];
  d["received"] = node["rx"] | 0U;
  d["gaps"] = node["gaps"] | 0U;
  d["duplicates"] = node["dup"] | 0U;
  d["nodes_online"] = shadow["nodes_online"] | 0;
  d["mqtt_connected"] = cloudOk;
  d["ip"] = WiFi.localIP().toString();
  d["rssi"] = WiFi.RSSI();
  d["uptime_s"] = millis() / 1000;
  d["published"] = published;
  d["publish_failed"] = failed;
  d["buffered"] = buffered;
  String body;
  serializeJson(d, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", body);
}

void webPoll() {
  static bool started = false;
  if (!started) {
    if (WiFi.status() != WL_CONNECTED) return;
    loadMQ135Calibration();
    server.on("/", HTTP_GET, [] {
      server.sendHeader("Cache-Control", "no-cache");
      server.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
    });
    server.on("/api/status", HTTP_GET, statusResponse);
    server.on("/api/calibrate/mq135", HTTP_POST, calibrateMQ135Response);
    server.on("/api/history", HTTP_GET, historyResponse);
    server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
    server.begin();
    started = true;
    Serial.printf("[web] http://%s/ (port 80), data source=NETPIE\n", WiFi.localIP().toString().c_str());
  }
  server.handleClient();
}
