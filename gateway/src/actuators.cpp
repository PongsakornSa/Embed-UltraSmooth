// actuators.cpp - Fan ("air conditioner") and window servo.
// Own LEDC-based servo driver: 50 Hz, pulse 500-2500 us mapped to 0-180 deg.
//
// Safety (M7): the window moves in small steps; it stops immediately when
//  - the limit switch in the moving direction is hit (ISR -> task notification)
//  - the move takes longer than WINDOW_MOVE_TIMEOUT_MS (jam) -> fault + alert
//  - the emergency stop is active
#include "app.h"
#include <esp_task_wdt.h>

static void servoWriteAngle(int deg) {
  deg = constrain(deg, 0, 180);
  uint32_t us = SERVO_MIN_US + (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * deg / 180;
  uint32_t duty = (uint32_t)((uint64_t)us * 65535 / 20000); // 16-bit, 20 ms period
  ledcWrite(LEDC_CH_SERVO, duty);
}

void fanSet(uint8_t pct) {
  pct = min<uint8_t>(pct, 100);
  const bool on = pct > 0;
  const uint8_t active = FAN_RELAY_ACTIVE_LOW ? LOW : HIGH;
  const uint8_t inactive = FAN_RELAY_ACTIVE_LOW ? HIGH : LOW;
  digitalWrite(PIN_FAN_RELAY, on ? active : inactive);
}

void fanBegin() {
  // Preload the inactive level before enabling the output to avoid a boot pulse.
  digitalWrite(PIN_FAN_RELAY, FAN_RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(PIN_FAN_RELAY, OUTPUT);
  fanSet(0);
}

void actuatorsBegin() {
  fanBegin();

  ledcSetup(LEDC_CH_SERVO, 50, 16);
  ledcAttachPin(PIN_SERVO, LEDC_CH_SERVO);
  // Boot state: window closed (safe default when we don't know outdoor air)
  servoWriteAngle(WINDOW_ANGLE_CLOSED);
}

static bool limitHit(int dir) {
  return dir > 0 ? digitalRead(PIN_LIMIT_OPEN) == LOW : digitalRead(PIN_LIMIT_CLOSED) == LOW;
}

void windowTask(void *) {
  esp_task_wdt_add(nullptr);
  int angle = WINDOW_ANGLE_CLOSED;
  uint32_t moveStart = 0;
  bool moving = false;
  int lastDir = 0;

  for (;;) {
    esp_task_wdt_reset();
    WindowTarget target;
    bool estop, fault;
    {
      Lock l;
      target = g.act.windowTarget;
      estop = g.act.estop;
      fault = g.act.windowFault; // latched after a jam until reset/manual press
    }
    int goal = (target == WindowTarget::OPEN) ? WINDOW_ANGLE_OPEN : WINDOW_ANGLE_CLOSED;
    int dir = (goal > angle) ? 1 : (goal < angle ? -1 : 0);

    bool stop = estop || fault || dir == 0 || limitHit(dir);
    if (!stop && moving && millis() - moveStart > WINDOW_MOVE_TIMEOUT_MS) {
      stop = true;
      { Lock l; g.act.windowFault = true; } // latch now so we don't retry
      Event e{EvType::WINDOW_FAULT, 0};
      xQueueSend(qEvents, &e, 0);
    }

    if (stop) {
      if (moving && dir != 0 && limitHit(dir)) angle = goal; // reached the end stop
      moving = false;
      {
        Lock l;
        g.act.windowMoving = false;
        g.act.windowAngle = angle;
      }
      // Sleep until a limit switch fires or 100 ms pass, then re-check target
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
      continue;
    }

    if (!moving || dir != lastDir) { moving = true; moveStart = millis(); } // new move or reversal
    lastDir = dir;
    angle += dir * WINDOW_STEP_DEG;
    if ((dir > 0 && angle > goal) || (dir < 0 && angle < goal)) angle = goal;
    servoWriteAngle(angle);
    {
      Lock l;
      g.act.windowMoving = true;
      g.act.windowAngle = angle;
    }
    // Wait one step; a limit-switch ISR wakes us early so we stop at once
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WINDOW_STEP_MS));
  }
}
