# Test plan

Fill in the **Result** column during testing (date, pass/fail, numbers, video link).

## A. Normal use

| # | Steps | Expected | Result |
|---|---|---|---|
| A1 | Power node + gateway, open dashboard | values update every 15 s, `nodes_online` = 1 | |
| A2 | Press AC button | fan starts < 1 s, `ac_src`=button, `ac_presses` +1 | |
| A3 | Press AC button again | fan stops, automation paused 30 min | |
| A4 | Press window button | window moves until limit switch, `window_src`=button | |
| A5 | Dashboard `{"ac":"on"}` | fan on, `ac_src`=dashboard | |
| A6 | Breathe on SCD40 / close room until CO2 > 1000, outdoor PM low | window opens | |
| A7 | Room warm + occupied, AI prob > 0.7 | AC turns on by itself, `ac_src`=ai | |
| A8 | Leave room empty 10 min | AC turns off | |

## B. Edge cases

| # | Steps | Expected | Result |
|---|---|---|---|
| B1 | Smoke / incense near PMS5003 (PM2.5 > 37.5) while window open | window closes, Telegram alert | |
| B2 | PM2.5 high **and** CO2 high | window stays closed (dust wins), fan can still run | |
| B3 | Wet the rain sensor while window manually opened | window closes anyway, alert | |
| B3b | Incense smoke near MQ-2 (after 1 min warm-up) while window open | window closes, `smoky`=1, Telegram alert | |
| B4 | Press window button during movement | window reverses smoothly | |
| B5 | Press AC + window buttons quickly many times | no crash, last press wins | |

## C. Failure conditions

| # | Steps | Expected | Result |
|---|---|---|---|
| C1 | Turn hotspot off 5 min, then on | control keeps working offline, data buffered, flushed after reconnect (`buffered` -> 0) | |
| C2 | Unplug sensor node | after 2 min: node offline alert, window closes (outdoor unknown) | |
| C3 | Unplug SCD40 | `co2` disappears, alert after 1 min, rest keeps working | |
| C4 | Pull gateway power, plug back | boots, `boots` +1, reconnects, Telegram "Gateway online" | |
| C5 | Block the window by hand | stops after 6 s, `window_fault`=1, alert | |
| C6 | Press E-stop | fan off, window frozen; press again to release | |
| C7 | Change hotspot channel / restart hotspot | node hops channels and finds gateway again | |

## D. Metrics (repeat each >= 10 times; report mean, min, max)

| Metric | Method |
|---|---|
| Node->gateway delivery (PDR) | node `[stats]` line every 60 s: `linkPDR`, `deliveryPDR`; gateway `nodes.*.rx/gaps/dup` over 24 h |
| Node->gateway round trip | node `[stats]` rtt avg/min/max |
| Sensor -> actuator latency | film PMS5003 + window with phone at 60 fps; frames from smoke reading to window motion (or Serial timestamps) |
| Sensor -> dashboard latency | compare `ts` in the JSON with the time the dashboard shows it |
| AI precision / recall | `python ai/train_model.py ai/data/*.csv` prints test-set precision/recall |
| Uptime | run >= 24 h; `uptime_s`, `boots`, NETPIE feed gaps |
| Sensor accuracy | SHT31 vs reference thermometer, PMS5003 vs Air4Thai station / reference meter |
