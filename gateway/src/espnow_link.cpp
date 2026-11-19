// espnow_link.cpp - Receive SensorPackets from nodes, ACK them, track every node.
// The receive callback runs in the Wi-Fi task: it only copies the packet into
// a queue. nodeTask does the work (ACK, duplicate detection, statistics).
#include "app.h"
#include <WiFi.h>
#include <esp_now.h>

struct RxPacket {
  uint8_t mac[6];
  SensorPacket pkt;
};
static_assert(sizeof(RxPacket) == sizeof(SensorPacket) + 6, "qPackets item size in main.cpp must match");

static void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(SensorPacket)) return;
  RxPacket rx;
  memcpy(rx.mac, mac, 6);
  memcpy(&rx.pkt, data, sizeof(SensorPacket));
  xQueueSend(qPackets, &rx, 0); // never block the Wi-Fi task
}

// Called by cloudTask after Wi-Fi is up, so ESP-NOW uses the hotspot channel.
void espnowBegin() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("[espnow] init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  Serial.printf("[espnow] ready on channel %d, gateway MAC %s\n", WiFi.channel(), WiFi.macAddress().c_str());
}

static void sendAck(const uint8_t *mac, const SensorPacket &p) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  AckPacket ack = {};
  ack.type = MSG_ACK;
  ack.version = PROTO_VERSION;
  memcpy(ack.node_id, p.node_id, NODE_ID_LEN);
  ack.seq = p.seq;
  esp_now_send(mac, (const uint8_t *)&ack, sizeof(ack));
}

static NodeState *findOrAddNode(const char *id) {
  for (int i = 0; i < g.nodeCount; i++)
    if (strncmp(g.nodes[i].id, id, NODE_ID_LEN) == 0) return &g.nodes[i];
  if (g.nodeCount >= MAX_NODES) return nullptr;
  NodeState *n = &g.nodes[g.nodeCount++];
  memset(n, 0, sizeof(*n));
  strncpy(n->id, id, NODE_ID_LEN - 1);
  Serial.printf("[espnow] new node %s\n", n->id);
  return n;
}

const NodeState *primaryOutdoorNode() {
  const NodeState *best = nullptr;
  for (int i = 0; i < g.nodeCount; i++) {
    const NodeState &n = g.nodes[i];
    if (n.simulated || n.received == 0 || millis() - n.lastSeenMs > NODE_OFFLINE_MS) continue;
    if (!best || n.lastSeenMs > best->lastSeenMs) best = &n;
  }
  return best;
}

const NodeState *mq135Node() {
  for (int i = 0; i < g.nodeCount; ++i) {
    const NodeState &n = g.nodes[i];
    if (n.received && !n.simulated && strcmp(n.id, MQ135_NODE_ID) == 0 && n.last.smoke_raw <= 4095)
      return &n;
  }
  return nullptr;
}

void nodeTask(void *) {
  RxPacket rx;
  for (;;) {
    if (xQueueReceive(qPackets, &rx, portMAX_DELAY) != pdTRUE) continue;
    SensorPacket &p = rx.pkt;
    if (p.type != MSG_SENSOR || p.version != PROTO_VERSION) continue;
    p.node_id[NODE_ID_LEN - 1] = '\0';

    sendAck(rx.mac, p); // ACK first: keeps node-side latency and retries low

    Lock l;
    NodeState *n = findOrAddNode(p.node_id);
    if (!n) continue;
    bool fresh = n->received == 0 || p.seq > n->last.seq ||
                 (p.seq < n->last.seq && p.uptime_ms < n->last.uptime_ms); // node rebooted
    if (!fresh) {
      n->duplicates++; // we got it before; our ACK was lost and the node re-sent
      continue;
    }
    if (n->received > 0 && p.seq > n->last.seq + 1) n->gaps += p.seq - n->last.seq - 1;
    n->last = p;
    n->lastSeenMs = millis();
    n->received++;
    n->simulated = p.sensor_flags & SF_SIMULATED;
    if (!n->simulated && strcmp(n->id, MQ135_NODE_ID) == 0)
      Serial.printf("[espnow] MQ135 node=%s seq=%lu raw=%u %s\n", n->id,
                    (unsigned long)p.seq, p.smoke_raw,
                    (p.sensor_flags & SF_SMOKE) ? "ready" : "warming-up");
  }
}
