# Message format

## 1. Sensor node -> gateway (ESP-NOW, binary)

Defined in [`common/include/protocol.h`](../common/include/protocol.h). Packed struct, little-endian.

| Field | Type | Meaning |
|---|---|---|
| type | uint8 | 1 = SENSOR, 2 = ACK |
| version | uint8 | protocol version (2) |
| node_id | char[8] | e.g. `out-01`, `sim-02` - lets one gateway serve many nodes |
| seq | uint32 | +1 per reading; used for PDR, duplicate and gap detection |
| uptime_ms | uint32 | node `millis()` at reading time |
| retries | uint8 | how many times this packet was re-sent |
| sensor_flags | uint8 | bit0 PMS ok, bit1 SHT ok, bit2 rain ok, bit3 smoke ok (warmed up), bit7 simulated node |
| pm25, pm10 | float | outdoor ug/m3 |
| temp_c, rh | float | outdoor temperature / humidity |
| rain_raw | uint16 | rain ADC (lower = wetter) |
| raining | uint8 | 1 = rain |
| smoke_raw | uint16 | MQ-2 ADC (higher = more smoke/gas) |
| smoky | uint8 | 1 = above `SMOKE_THRESHOLD` (valid only when flag bit3 is set) |

Gateway replies with an **AckPacket** `{type=2, version, node_id, seq}`.
No ACK within 150 ms -> node re-sends (max 3), then keeps the packet in a
backlog and sends it later, oldest first.

## 2. Gateway -> cloud (NETPIE, MQTT over TLS, JSON)

Topic `@shadow/data/update`, every 5 s:

```json
{"data":{
  "ts":1732000000, "in_temp":29.41, "in_rh":61.2, "co2":812, "lux":420, "occupied":1,
  "out_pm25":42, "out_pm10":58, "out_temp":31.20, "out_rh":68.0, "raining":0, "rain_raw":3980, "out_smoke":410, "smoky":0,
  "ac_on":1, "fan_pct":73, "ac_src":"ai", "ac_manual":0,
  "window_open":0, "window_angle":0, "window_src":"rule", "window_fault":0, "estop":0,
  "ai_prob":0.812, "ai_valid":1, "ac_presses":14,
  "nodes":{"out-01":{"age_s":4,"rx":1530,"gaps":3,"dup":7,"sim":0}},
  "nodes_online":1, "uptime_s":86400, "boots":3, "heap":143000, "buffered":0
}}
```

When the cloud is unreachable the same JSON is appended to `/buffer.jsonl`
(LittleFS, max 256 KB) and published later; `ts` keeps the real time.

## 3. Cloud -> gateway (commands)

Topic `@msg/cmd`:

| Payload | Effect |
|---|---|
| `{"ac":"on"}` / `{"ac":"off"}` | manual AC for 30 min |
| `{"ac":"auto"}` | back to automatic |
| `{"window":"open"}` / `"close"` / `"auto"` | same for the window |
| `{"estop":"reset"}` | release emergency stop, clear window fault |

## 4. Alerts

Topic `@msg/alert` (plain text) + Telegram message. 15 min cooldown per alert type.

## 5. Serial log (AI training data)

One line per minute on USB serial:

```
LOG,epoch,in_temp,in_rh,co2,occupied,lux,out_temp,hour_sin,hour_cos,slope,out_pm25,ac_on,fan,window,presses,ai_prob
EVT,ac_manual_on,epoch
```

## 6. MQ-135 receiver / local web server

No wire-format change is required for the current node firmware. On the selected
node (`MQ135_NODE_ID`, default `out-01`), connect MQ-135 to the existing GPIO34
analog input. Protocol v2's `smoke_raw` field carries its 12-bit ADC reading;
`SF_SMOKE` reports completion of the node firmware's warm-up timer. This flag
does not certify calibration or air safety. Simulated nodes are excluded.

`GET /api/status` returns `node_id`, `source: "netpie"`, `mq135_raw`,
`last_mq135_raw`, `node_online`, `sample_age_s`, `sequence`, `sample_id`,
`warmed_up`, packet counters and available node sensor values. All sensor values
are mapped from a NETPIE REST shadow response cached by the board every 5 s.
NETPIE errors, shadows older than 45 s, and stale/missing node samples produce
null current readings. `netpie_connected` and `netpie_http` report REST status.
ESP-NOW data is published to NETPIE every 5 s, then read back for the page.
The default web-only role does not initialize local sensors or actuators and
does not subscribe to actuator commands. NETPIE forwarding remains enabled.

## Update intervals

Node samples and sends every 2 seconds (`SEND_INTERVAL_MS=2000`). Gateway
publishes its latest snapshot every 5 seconds (`PUBLISH_PERIOD_MS=5000`).
Each change takes effect only after that board is flashed. Intermediate node
readings may be replaced before the next gateway snapshot; Feed records the
snapshots that reach NETPIE, rather than every 2-second node measurement.
