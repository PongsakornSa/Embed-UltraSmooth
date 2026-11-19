# Cloud, dashboard and alerts setup

## NETPIE 2020

1. Sign up at https://portal.netpie.io -> create a **Project** -> create a **Device**
   (e.g. `smart-classroom-gw`).
2. Device -> **Key**: copy Client ID, Token, Secret into `gateway/include/secrets.h`.
3. Device -> **Feed**: turn the feed ON so every shadow update is stored
   (this is the raw-data cloud storage, M5). Add the numeric fields to the
   **Schema** with type `number`:
   `in_temp in_rh co2 lux occupied out_pm25 out_pm10 out_temp out_rh raining out_smoke smoky
   ac_on fan_pct window_open ai_prob ac_presses nodes_online`.
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
