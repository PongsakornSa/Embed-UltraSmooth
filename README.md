# Smart Classroom - Embedded Systems Final Project 2026

Smart classroom that measures indoor and outdoor air, opens/closes the window
depending on outdoor dust (PM2.5), rain and indoor CO2, and runs the "air
conditioner" (a 12 V fan in this prototype). An on-device AI learns **when people
usually press the AC button** and turns the AC on a few minutes earlier
(pre-cooling). Manual buttons and the dashboard always override automation.

```
 [Sensor node - outdoor]                [Gateway - indoor]                       [Cloud]
  PMS5003  PM2.5/PM10                    SCD40 CO2, SHT31, BH1750, PIR            NETPIE (MQTT/TLS)
  DHT22    temp/RH      --ESP-NOW-->     FreeRTOS tasks, decision logic   --->    shadow + feed (storage)
  Rain, MQ-2 smoke      <---ACK---       on-device AI (MLP)                       web dashboard
                                         Fan "AC" (relay GPIO)                    Telegram alerts
                                         Window servo + limit switches    <---    commands (@msg/cmd)
                                         Buttons, E-stop, OLED
```

## Folder layout

| Path | What |
|---|---|
| `common/include/protocol.h` | node <-> gateway message format |
| `common/include/drivers/` | **our own** drivers: SHT31, SCD4x, BH1750 (I2C), PMS5003 (UART); node DHT22 uses the Adafruit DHT library |
| `node/` | PlatformIO project, sensor node firmware |
| `gateway/` | PlatformIO project, gateway firmware |
| `ai/train_model.py` | trains the AC-prediction model, writes `gateway/include/ac_model.h` |
| `ai/serial_logger.py` | records the gateway's training data from USB serial |
| `dashboard/` | our own web dashboard (live values, history charts, control buttons) |
| `docs/` | wiring, message format, cloud/dashboard setup, test plan |

## Quick start

1. Install VS Code + PlatformIO (or `pip install platformio`).
2. Copy `gateway/include/secrets.example.h` to `gateway/include/secrets.h` and fill in
   Wi-Fi, NETPIE and Telegram values ([docs/cloud_setup.md](docs/cloud_setup.md)).
   `secrets.h` is git-ignored - never commit it.
3. Wire the boards ([docs/wiring.md](docs/wiring.md)).
4. Flash:
   ```
   cd gateway && pio run -t upload && pio device monitor
   cd node    && pio run -e node_out01 -t upload && pio device monitor
   ```
   `node_out01_sim` = real node that also sends two simulated nodes (`sim-02`, `sim-03`).
   `node_out02` = second physical node.
5. Local ESP32 dashboard: connect your phone/computer to the same Wi-Fi as the
   gateway and open the address printed as `[web] http://<IP>/` in Serial Monitor.
   The board serves the page itself on port 80, with no external assets or NETPIE
   keys in the browser. It shows MQ-135 ADC read back from NETPIE Shadow, sensor availability, connection
   and device status, refreshing every 5 seconds. `GET /api/status` returns JSON;
   unavailable sensor measurements are `null`. The page is read-only. Network
   client isolation on some hotspots can prevent access between devices.
   The default `GATEWAY_WEB_ONLY=1` role receives ESP-NOW and serves HTTP; local
   sensor, OLED, control, AI and actuator tasks are disabled. NETPIE forwarding
   remains enabled. MQ-135 is connected to GPIO34 on the **node**, not the gateway.
   Existing protocol v2 `smoke_raw` is interpreted as MQ-135 raw ADC for
   `MQ135_NODE_ID` (`out-01`, configurable in `gateway/include/config.h`). The node
   must run `node_out01` with real readings. Mock mode cannot provide real MQ-135
   measurements. The data flow is node -> ESP-NOW -> gateway -> MQTT/TLS -> NETPIE -> HTTPS REST
   -> board web server -> browser. The board reads NETPIE every 5 seconds; all
   sensor values in `/api/status` come from that response, never directly from
   local ESP-NOW state. Keys stay on the board. An unavailable API or stale
   shadow clears current readings to null. A warm-up flag is shown separately
   from ADC counts.
   The page also queries NETPIE Feed for 1/6/24-hour MQ-135 history. Enable Feed
   and create an enabled Number Datatag with Attr `mq135_raw` to start recording.
   The shared chart also prepares temperature, light, sound and flame fields;
   raw history preserves recorded peaks and can be downloaded as CSV.
6. Later gateway updates over Wi-Fi (OTA): set `GW_OTA_PASSWORD` env var, then
   `pio run -e gateway_ota -t upload`.

The node finds the gateway by itself: it hops Wi-Fi channels until the gateway
ACKs (the gateway's ESP-NOW channel follows its hotspot channel).

## AI workflow

1. Run the gateway in the classroom and record data:
   `python ai/serial_logger.py COM5` (one row per minute; every manual "AC on"
   press is a label).
2. Train and export: `python ai/train_model.py ai/data/*.csv`
   (add `--synthetic` while real data is still small). It prints test-set
   precision/recall and rewrites `gateway/include/ac_model.h`.
3. Re-flash the gateway (USB or OTA).

The current `ac_model.h` was trained on **synthetic data only** (bootstrap) -
retrain on real logs before the demo.

## Requirement mapping

| Item | Where |
|---|---|
| M1 sensors (7 types) | node: PMS5003, DHT22, rain, MQ-2 smoke; gateway: SCD40, SHT31, BH1750, PIR/LD2410 |
| M2 actuator | window servo, fan (DC motor) - `gateway/src/actuators.cpp` |
| M3 node -> gateway | ESP-NOW + ACK - `node/src/main.cpp`, `gateway/src/espnow_link.cpp` |
| M4 AI closed loop | `gateway/src/ai.cpp` -> `control.cpp` turns AC on when prob >= 0.7 |
| M5 cloud + dashboard | NETPIE shadow/feed - `gateway/src/cloud.cpp`; web dashboard - `dashboard/` |
| M6 notification | Telegram - `cloud.cpp` `raiseAlert()` |
| M7 safety | limit switches, 6 s move timeout, E-stop, 12 V only |
| M8 Git | this repo; every member commits their own part |
| Scalability | `node_id` in every packet, up to 6 nodes, simulated nodes env |
| E1 RTOS | 7 tasks, queues, mutex, task notifications - `gateway/include/app.h` |
| E2 ISR | buttons, presence, limit switches - `gateway/src/inputs.cpp` |
| E4 reliable comm | ACK/retry + node backlog + gateway LittleFS buffer |
| E5 security | MQTT over TLS + credentials, secrets git-ignored |
| E6 OTA | ArduinoOTA, `gateway_ota` env |
| E7 on-device AI | MLP inference on the ESP32 - `gateway/src/ai.cpp` |
| E8 own drivers | `common/include/drivers/` (I2C commands + CRC, UART frame parser) |

## Known limitations

- The fan stands in for a real air conditioner; a real deployment would send the
  AC's IR remote code instead (IR does not count as an actuator, the window does).
- Telegram HTTPS uses encryption without certificate pinning (`setInsecure`).
- Buffered records are re-published to the shadow, so right after a reconnect the
  dashboard may briefly show older values (use `ts`).

## AI tools declaration

Firmware skeleton, drivers and scripts were drafted with Claude Code (Anthropic) and
reviewed/modified by the team. Each member must be able to explain their own part.
