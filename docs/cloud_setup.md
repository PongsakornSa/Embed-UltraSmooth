# Cloud, dashboard and alerts setup

## NETPIE 2020

1. Sign up at https://portal.netpie.io -> create a **Project** -> create a **Device**
   (e.g. `smart-classroom-gw`).
2. Device -> **Key**: copy Client ID, Token, Secret into `gateway/include/secrets.h`.
3. Device -> **Feed** -> **+ Add datatag**: choose the Shadow field in Data Path,
   Data Type **Number**, and set **Attr** exactly to the desired field name.
   Choose TTL, create the Datatag and enable **DataTag Enable**. For MQ-135 use
   `mq135_raw`. To record other values, create matching Datatags for their fields.
   Schema alone does not enable historical recording on the current NETPIE UI.
4. Export the feed as CSV any time (Feed -> Export) for the report graphs.

### TLS certificate (E5)

```
openssl s_client -connect mqtt.netpie.io:8883 -showcerts
```

Copy the last certificate in the chain (root CA) into `NETPIE_ROOT_CA` in
`secrets.h`. With an empty string the link is still encrypted but the server
is not verified - the report should say which one you used.

## Dashboard (our own web page, `dashboard/`)

A static page (HTML + JS, Chart.js) that talks to the NETPIE REST API directly:

| Part | API |
|---|---|
| Live values (every 5 s) | `GET https://api.netpie.io/v2/device/shadow/data` |
| History charts (1 / 6 / 24 h) | `POST https://feed.netpie.io/api/v1/datapoints/query` (needs Feed ON + schema, step 3 above) |
| Control buttons | `PUT https://api.netpie.io/v2/device/message?topic=cmd`, body = JSON from section 3 of `message_format.md` |

All requests use the header `Authorization: Device <ClientID>:<Token>`.

Run it:

```
cd dashboard
python -m http.server 8080
```

Open http://localhost:8080 and enter the gateway's Client ID and Token in the
settings dialog (stored in this browser's localStorage only). Or copy
`config.local.example.js` to `config.local.js` (git-ignored) to skip the dialog.

Keys are never in the repo. Do not host the page publicly with keys built in:
anyone with the token can read the data and send commands.

## Dashboard (NETPIE Freeboard, alternative)

Project -> Freeboard -> new board -> Datasource: NETPIE device (the gateway).

| Pane | Widgets |
|---|---|
| Indoor | Gauge `in_temp`, Gauge `co2` (green < 800, yellow < 1000, red), Text `in_rh`, `lux`, Indicator `occupied` |
| Outdoor | Gauge `out_pm25` (colour by Thai AQI bands), Text `out_temp`, Indicator `raining` |
| Actuators | Indicator `ac_on` + Text `fan_pct` / `ac_src`; Indicator `window_open` + `window_src`; Indicator `estop`, `window_fault` |
| AI | Gauge `ai_prob` (0-1), Text `ac_presses` |
| Control | Buttons publishing to `@msg/cmd`: AC on / off / auto, Window open / close / auto, E-stop reset |
| History | Chart `in_temp`, `co2`, `out_pm25` (feed datasource) |
| System | Text `nodes_online`, `uptime_s`, `boots`, `buffered` |

Button example (Freeboard "button" widget, onClick; the datasource name here is
`gw` - check the NETPIE Freeboard docs for the exact call in your version):
```js
netpie["gw"].publish("@msg/cmd", JSON.stringify({ac: "on"}))
```

## Telegram alerts (M6)

1. In Telegram talk to **@BotFather** -> `/newbot` -> copy the token.
2. Send any message to your bot, then open
   `https://api.telegram.org/bot<TOKEN>/getUpdates` and copy `chat.id`
   (use a group chat id to alert the whole team).
3. Put both into `secrets.h`.

## ESP32-hosted history page

The embedded page reads live data back from NETPIE Shadow and historical
`mq135_raw` points from NETPIE Feed through the board. Keys are never sent to
the browser. Select 1, 6 or 24 hours; the chart uses raw recorded samples with no averaging.
The history refreshes every minute and is cached per range for one minute.
`GET /api/history?hours=1` (also 6 or 24) returns 202 while loading, then JSON
with `state`, `hours`, `aggregation: "raw"`, `next_after` and raw `[timestamp_ms, value]` points.

Create and enable a Number Datatag for `mq135_raw` on the same gateway device.
Historical samples only exist after recording is configured.
An empty chart is expected until the field is configured and data arrives.
Official Feed API: https://docs.netpie.io/en/device-api.html

## Shared sensor chart (prepared fields)

One chart supports five sensor selectors and 1/6/24-hour Feed history:

| Sensor | NETPIE Shadow field / Feed Attr | Unit |
|---|---|---|
| MQ-135 | `mq135_raw` | ADC 0–4095 |
| Temperature | `temperature_c` | °C |
| Light | `light_lux` | lux |
| Sound | `sound_raw` | raw ADC, not dB |
| Flame | `flame_raw` | raw ADC, not a fire alarm |

For history, use Feed -> Add datatag, select the matching Shadow Data Path,
Data Type Number, set Attr exactly as in the table, choose TTL and enable the
Datatag. Current NETPIE uses Datatags rather than Schema alone to enable storage.

The current protocol v2 node supplies MQ-135 through `smoke_raw` and temperature
through `temp_c` with SF_SHT. The gateway publishes `temperature_c` and its sample
timestamp `temperature_c_ts`. Light, sound and flame are prepared web fields only:
the existing node firmware does not transmit them. Future sensor publishers must
send each value and its `<field>_ts` as Unix seconds (e.g. `light_lux_ts`). Missing
or stale samples stay null; no sensor values or dB/ppm conversions are invented.
`GET /api/history?metric=light&hours=6` selects a metric and time range. Allowed
metrics are mq135, temperature, light, sound, flame; missing metric defaults to
mq135 for compatibility. History response caching is bounded to 24 KB.

## Raw history update

History now queries raw Feed samples with no averaging or other aggregator.
The board returns at most 300 points per page to limit RAM usage. The browser
loads subsequent pages using `after`/`until` epoch-millisecond query parameters
and `next_after` from the response, combining points into the single chart.
Min/max/mean summaries are computed from recorded raw samples in the selected
time window. CSV download exports the complete loaded series with timestamps.
A spike can only appear if it was actually recorded by NETPIE; UI polling does
not reconstruct samples the node/gateway never sent.

## Inspecting chart samples

Hover or tap the raw chart to snap to the nearest recorded sample and show its
value, unit and full timestamp. The highlighted dot follows the sample itself,
with a vertical guide. Tooltips stay within the chart edges. Tab to focus the
chart and use Left/Right, Home/End to inspect samples, or Escape to dismiss.
Changing sensor or time range clears the previous selection.

## Separate charts and shared Time menu

The dashboard shows five independent sensor cards: MQ-135, temperature, light,
sound and flame. The Time dropdown above them changes all charts to the same
time window: Last 1 hour, 6 hours, 24 hours, 7 days or 30 days. Each card has its
own live reading, raw history, hover/touch inspection, statistics and CSV export.
Selecting a new window cancels previous requests for every chart. Long histories
remain raw and are loaded in 300-point pages; the board schedules sensor queries
in round-robin order and keeps its 24 KB response-cache limit. History refresh
waits for current downloads to finish rather than aborting long-window queries.
`/api/history` accepts hours 1, 6, 24, 168 or 720 for these windows.

The current firmware runs NETPIE Shadow/Feed reads in a dedicated `webdata`
task, independent of MQTT reconnects. History requests wait for NTP time sync
before using absolute timestamps, avoiding malformed ranges during startup.

Node source configuration now reads/sends every 2 seconds. Gateway NETPIE
publish configuration is 5 seconds. The node must be flashed separately for
its new interval to apply; updating only the gateway leaves the deployed node's
existing interval unchanged. NETPIE readback polls every 2 seconds and the browser live view every 1 second.

## Current tuned timing

| Stage | Interval |
|---|---|
| Node sampling/sending | 2 s (source/firmware prepared; requires node flash) |
| Gateway MQTT snapshot | 5 s |
| NETPIE Shadow readback | 2 s |
| Browser live value read | 1 s |
| Recent raw Feed history | 10 s; incremental |
| Empty, inactive history fields | 60 s |

The first load or changing Time fetches the selected range. Subsequent updates
request only the new tail plus 20 seconds of overlap, merge by timestamp, and
keep the existing trace and raw peaks. A successful-coverage checkpoint ensures
failed requests are retried without skipping the missed interval. One busy
chart does not prevent other charts from updating. Every measurement displayed
still comes back from NETPIE; local ESP-NOW state is not used as a shortcut.
