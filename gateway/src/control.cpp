// control.cpp - Real-time decision logic on the gateway.
//
// Priority of decisions (highest first):
//   1. Emergency stop          -> everything off, window frozen
//   2. Rain or smoke outside   -> close window (even over a manual command)
//   3. Manual command (button / dashboard) -> obeyed for MANUAL_HOLD_MS
//   4. Outdoor PM2.5 bad / unknown -> close window
//   5. AC running              -> close window
//   6. AI predicts the user will turn the AC on -> pre-cool (AC on)
//   7. CO2 high or free cooling possible -> open window
//   8. Room empty for EMPTY_AC_OFF_MS -> AC off
#include "app.h"
#include <esp_task_wdt.h>

static void setAc(bool on, Source src) {
  if (g.act.acOn == on) return;
  g.act.acOn = on;
  g.act.acSource = src;
  g.act.acOnSinceMs = on ? millis() : 0;
  Serial.printf("[ctrl] AC %s (%s)\n", on ? "ON" : "OFF", sourceName(src));
}

static void setWindow(WindowTarget t, Source src) {
  if (g.act.windowTarget == t) return;
  g.act.windowTarget = t;
  g.act.windowSource = src;
  Serial.printf("[ctrl] window -> %s (%s)\n", t == WindowTarget::OPEN ? "OPEN" : "CLOSED", sourceName(src));
}

// A manual "AC on" while the AC was off is a training label for the AI.
static void countManualOn() {
  g.stats.acManualOnPresses++;
  g.stats.pressesSinceLog++;
  Serial.printf("EVT,ac_manual_on,%lu\n", (unsigned long)time(nullptr));
}

static void manualAc(bool on, Source src) {
  if (on && !g.act.acOn) countManualOn();
  setAc(on, src);
  g.act.acManualUntil = millis() + MANUAL_HOLD_MS;
}

static void manualWindow(WindowTarget t, Source src) {
  g.act.windowFault = false; // a person is looking at it: allow retry
  setWindow(t, src);
  g.act.windowManualUntil = millis() + MANUAL_HOLD_MS;
}

static void handleEvent(const Event &e) {
  Lock l;
  switch (e.type) {
    case EvType::BTN_AC:
      if (!g.act.estop) manualAc(!g.act.acOn, Source::BUTTON);
      break;
    case EvType::BTN_WINDOW: {
      if (g.act.estop) break;
      WindowTarget t = g.act.windowTarget == WindowTarget::OPEN ? WindowTarget::CLOSED : WindowTarget::OPEN;
      manualWindow(t, Source::BUTTON);
      break;
    }
    case EvType::BTN_ESTOP:
      g.act.estop = !g.act.estop; // press again to release
      if (g.act.estop) setAc(false, Source::SAFETY);
      raiseAlert("estop", g.act.estop ? "EMERGENCY STOP pressed: fan off, window frozen"
                                      : "Emergency stop released");
      break;
    case EvType::CMD_AC:
      if (g.act.estop) break;
      if (e.value == 2) g.act.acManualUntil = 0; // back to automatic
      else manualAc(e.value == 1, Source::DASHBOARD);
      break;
    case EvType::CMD_WINDOW:
      if (g.act.estop) break;
      if (e.value == 2) { g.act.windowManualUntil = 0; g.act.windowFault = false; }
      else manualWindow(e.value == 1 ? WindowTarget::OPEN : WindowTarget::CLOSED, Source::DASHBOARD);
      break;
    case EvType::CMD_ESTOP_RESET:
      g.act.estop = false;
      g.act.windowFault = false;
      raiseAlert("estop", "Emergency stop reset from dashboard");
      break;
    case EvType::PRESENCE:
      g.in.lastMotionMs = millis();
      g.in.occupied = true;
      break;
    case EvType::WINDOW_FAULT:
      raiseAlert("winfault", "Window did not reach its end position in %d ms - check for obstruction",
                 WINDOW_MOVE_TIMEOUT_MS);
      break;
  }
}

static uint8_t fanPercentFor(float tempC) {
  if (isnan(tempC)) return 100;
  float x = (tempC - FAN_TEMP_LOW) / (FAN_TEMP_HIGH - FAN_TEMP_LOW);
  x = constrain(x, 0.0f, 1.0f);
  return (uint8_t)(FAN_MIN_PCT + x * (100 - FAN_MIN_PCT));
}

static void evaluate() {
  Lock l;
  uint32_t now = millis();
  ActState &a = g.act;
  IndoorState &in = g.in;
  in.occupied = in.lastMotionMs != 0 && now - in.lastMotionMs < OCCUPANCY_HOLD_MS;

  if (a.estop) {
    setAc(false, Source::SAFETY);
    if (a.fanPct != 0) { a.fanPct = 0; fanSet(0); }
    return;
  }

  // ---- Outdoor situation (from the sensor node)
  const NodeState *out = primaryOutdoorNode();
  bool pmKnown = out && (out->last.sensor_flags & SF_PMS) && !isnan(out->last.pm25);
  float pm = pmKnown ? out->last.pm25 : NAN;
  bool raining = out && out->last.raining;
  bool smoky = out && (out->last.sensor_flags & SF_SMOKE) && out->last.smoky;
  float outT = (out && (out->last.sensor_flags & SF_SHT)) ? out->last.temp_c : NAN;

  // ---- AC (fan)
  if (now >= a.acManualUntil) {
    uint32_t emptyFor = in.lastMotionMs ? now - in.lastMotionMs : UINT32_MAX;
    if (a.acOn && !in.occupied && emptyFor > EMPTY_AC_OFF_MS) {
      setAc(false, Source::RULE);
    } else if (!a.acOn && in.occupied) {
      if (g.ai.valid && g.ai.prob >= AI_ON_THRESHOLD) setAc(true, Source::AI);            // pre-cool
      else if (!g.ai.valid && in.tempC >= TEMP_RULE_AC_ON) setAc(true, Source::RULE);     // fallback
    }
  }
  uint8_t pct = a.acOn ? fanPercentFor(in.tempC) : 0;
  if (pct != a.fanPct) { a.fanPct = pct; fanSet(pct); }

  // ---- Window
  if (raining && a.windowTarget == WindowTarget::OPEN) {
    a.windowManualUntil = 0; // rain beats a manual "open"
    setWindow(WindowTarget::CLOSED, Source::SAFETY);
    raiseAlert("rain", "Rain detected outside - window closed");
  }
  if (smoky) { // smoke outside (burning, fire): close even over a manual "open"
    if (a.windowTarget == WindowTarget::OPEN) {
      a.windowManualUntil = 0;
      setWindow(WindowTarget::CLOSED, Source::SAFETY);
    }
    raiseAlert("smoke", "Smoke detected outside (MQ-2 raw %u) - window kept closed", out->last.smoke_raw);
  }
  if (now >= a.windowManualUntil && !a.windowFault) {
    bool isOpen = a.windowTarget == WindowTarget::OPEN;
    // hysteresis: close at PM25_BAD, reopen only below PM25_GOOD
    bool outsideBad = !pmKnown || raining || smoky || (isOpen ? pm >= PM25_BAD : pm > PM25_GOOD);
    if (outsideBad || a.acOn) {
      if (isOpen && pmKnown && pm >= PM25_BAD)
        raiseAlert("pm", "Outdoor PM2.5 %.0f ug/m3 - window closed", pm);
      setWindow(WindowTarget::CLOSED, Source::RULE);
    } else {
      bool co2High = in.scdOk && in.co2 >= CO2_HIGH;
      bool co2Ok = !in.scdOk || in.co2 <= CO2_OK;
      bool freeCool = in.occupied && in.tempC >= FREE_COOL_MIN_IN && !isnan(outT) &&
                      outT <= in.tempC - FREE_COOL_DELTA;
      if (co2High || freeCool) setWindow(WindowTarget::OPEN, Source::RULE);
      else if (isOpen && co2Ok) setWindow(WindowTarget::CLOSED, Source::RULE);
    }
  }

  // ---- Alerts on conditions
  if (in.scdOk && in.co2 >= CO2_ALERT) raiseAlert("co2", "Indoor CO2 %u ppm - please ventilate", in.co2);
  if (!in.scdOk && now > 60000) raiseAlert("scd", "CO2 sensor not responding");
  for (int i = 0; i < g.nodeCount; i++) {
    const NodeState &n = g.nodes[i];
    if (n.received && now - n.lastSeenMs > NODE_OFFLINE_MS) {
      char key[16];
      snprintf(key, sizeof(key), "off-%s", n.id);
      raiseAlert(key, "Sensor node %s offline for %lu s", n.id, (now - n.lastSeenMs) / 1000);
    }
  }
}

void controlTask(void *) {
  esp_task_wdt_add(nullptr);
  vTaskDelay(pdMS_TO_TICKS(3000)); // let sensors produce a first reading
  for (;;) {
    esp_task_wdt_reset();
    Event e;
    // React to an event immediately, otherwise re-evaluate once per second
    if (xQueueReceive(qEvents, &e, pdMS_TO_TICKS(1000)) == pdTRUE) handleEvent(e);
    evaluate();
  }
}
