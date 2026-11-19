// The browser reads cached NETPIE REST responses, never local ESP-NOW readings.
#include "app.h"
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "web_page.h"
#include "web_metrics.h"
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
static const uint16_t historyHours[] = {1, 6, 24, 168, 720};
static constexpr size_t HISTORY_RANGE_COUNT = sizeof(historyHours) / sizeof(historyHours[0]);
static constexpr size_t HISTORY_CACHE_COUNT = WEB_METRIC_COUNT * HISTORY_RANGE_COUNT;
static HistoryCache historyCache[HISTORY_CACHE_COUNT];
static uint32_t historyPending = 0;
static_assert(HISTORY_CACHE_COUNT <= 32, "history pending mask capacity");

// Runs in webDataTask, serially with shadow reads, never in HTTP handlers.
void webHistoryRefresh() {
  int slot = -1;
  uint64_t startMs = 0, endMs = 0;
  {
    Lock l;
    static size_t nextSlot = 0;
    for (size_t offset = 0; offset < HISTORY_CACHE_COUNT; ++offset) {
      size_t i = (nextSlot + offset) % HISTORY_CACHE_COUNT;
      if (historyPending & (1UL << i)) {
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
    historyPending &= ~(1UL << slot);
    historyCache[slot].body = body;
    historyCache[slot].storedAt = millis();
    historyCache[slot].ok = ok;
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
  Serial.printf("[web] NETPIE Feed %s %uh HTTP=%d points=%u\n", spec.id, historyHours[range], status, (unsigned)points.size());
}

void webDataTask(void *) {
  for (;;) {
    webCloudRefresh();
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
    if (hours == String(historyHours[i])) range = i;
  if (range < 0) {
    server.send(400, "application/json", "{\"error\":\"hours must be 1, 6, 24, 168 or 720\"}");
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
  uint64_t rangeMs = (uint64_t)historyHours[range] * 3600000;
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
      if (cache.endMs && ((!cache.body.isEmpty() && millis() - cache.storedAt < ttl) || (historyPending & (1UL << slot)))) {
        startMs = cache.startMs; endMs = cache.endMs;
      } else { endMs = nowMs; startMs = endMs - rangeMs; }
    }
    bool matches = cache.startMs == startMs && cache.endMs == endMs;
    if (matches && !cache.body.isEmpty() && millis() - cache.storedAt < ttl) body = cache.body;
    else {
      cache.startMs = startMs; cache.endMs = endMs; cache.body = "";
      historyPending |= 1UL << slot;
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
  uint32_t fetchedAt, published, failed, buffered;
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
  d["last_mq135_raw"] = shadow["mq135_raw"];
  d["sample_age_s"] = nullptr;
  d["shadow_age_s"] = nullptr;
  d["sequence"] = shadow["mq135_seq"];
  d["sample_id"] = shadow["mq135_sample_id"];
  d["warmed_up"] = shadow["mq135_warmed_up"] | false;
  d["temperature"] = nullptr;
  d["humidity"] = nullptr;
  d["pm25"] = nullptr;
  d["pm10"] = nullptr;
  uint32_t ts = shadow["ts"] | 0U;
  time_t now = time(nullptr);
  bool shadowFresh = false;
  if (ts > 1600000000U && now > 1600000000 && (int64_t)now - ts >= -5) {
    uint32_t age = now > ts ? (uint32_t)(now - ts) : 0;
    d["shadow_age_s"] = age;
    if (shadow["mq135_age_s"].is<uint32_t>())
      d["sample_age_s"] = age + shadow["mq135_age_s"].as<uint32_t>();
    shadowFresh = age < 45;
  }
  bool sourceMatches = strcmp(shadow["mq135_node"] | "", MQ135_NODE_ID) == 0;
  bool fresh = netpieOk && shadowFresh && sourceMatches &&
               (shadow["mq135_online"] | 0) == 1 &&
               shadow["mq135_raw"].is<uint16_t>() &&
               shadow["mq135_raw"].as<uint16_t>() <= 4095 &&
               d["sample_age_s"].is<uint32_t>() &&
               d["sample_age_s"].as<uint32_t>() < 45;
  d["node_online"] = fresh;
  if (fresh) {
    d["mq135_raw"] = shadow["mq135_raw"];
    d["temperature"] = shadow["mq135_temp"];
    d["humidity"] = shadow["mq135_rh"];
    d["pm25"] = shadow["mq135_pm25"];
    d["pm10"] = shadow["mq135_pm10"];
  }
  JsonObject measurements = d["measurements"].to<JsonObject>();
  JsonObject ages = d["measurement_age_s"].to<JsonObject>();
  for (const WebMetric &metric : WEB_METRICS) {
    measurements[metric.id] = nullptr;
    ages[metric.id] = nullptr;
    if (strcmp(metric.id, "mq135") == 0) {
      measurements[metric.id] = d["mq135_raw"];
      ages[metric.id] = d["sample_age_s"];
      continue;
    }
    // Each future sensor must timestamp its own real sample, preventing retained
    // Shadow fields from appearing live when only another sensor is updating.
    String timestampField = String(metric.field) + "_ts";
    uint32_t sampleTs = shadow[timestampField] | 0U;
    if (sampleTs > 1600000000U && now > 1600000000 && (int64_t)now - sampleTs >= -5) {
      uint32_t age = now > sampleTs ? (uint32_t)(now - sampleTs) : 0;
      ages[metric.id] = age;
      if (netpieOk && age < 45 && shadow[metric.field].is<double>()) {
        double value = shadow[metric.field].as<double>();
        if (isfinite(value) && value >= metric.min && value <= metric.max) measurements[metric.id] = value;
      }
    }
  }
  d["temperature"] = measurements["temperature"];
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
    server.on("/", HTTP_GET, [] {
      server.sendHeader("Cache-Control", "no-cache");
      server.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
    });
    server.on("/api/status", HTTP_GET, statusResponse);
    server.on("/api/history", HTTP_GET, historyResponse);
    server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
    server.begin();
    started = true;
    Serial.printf("[web] http://%s/ (port 80), data source=NETPIE\n", WiFi.localIP().toString().c_str());
  }
  server.handleClient();
}
