// inputs.cpp - Interrupt-driven inputs (E2).
// ISRs only push a pin id into a queue (deferred processing). inputTask does
// the debouncing and converts raw edges into Events for controlTask.
// Limit switches are special: their ISR notifies windowTask directly so the
// servo stops with minimum delay (safety).
#include "app.h"

static void IRAM_ATTR isrBtnAc() { uint8_t p = ISR_BTN_AC; BaseType_t w = pdFALSE; xQueueSendFromISR(qIsr, &p, &w); portYIELD_FROM_ISR(w); }
static void IRAM_ATTR isrBtnWin() { uint8_t p = ISR_BTN_WINDOW; BaseType_t w = pdFALSE; xQueueSendFromISR(qIsr, &p, &w); portYIELD_FROM_ISR(w); }
static void IRAM_ATTR isrBtnEstop() { uint8_t p = ISR_BTN_ESTOP; BaseType_t w = pdFALSE; xQueueSendFromISR(qIsr, &p, &w); portYIELD_FROM_ISR(w); }
static void IRAM_ATTR isrPresence() { uint8_t p = ISR_PRESENCE; BaseType_t w = pdFALSE; xQueueSendFromISR(qIsr, &p, &w); portYIELD_FROM_ISR(w); }

static void IRAM_ATTR isrLimit() {
  BaseType_t w = pdFALSE;
  if (hWindowTask) vTaskNotifyGiveFromISR(hWindowTask, &w);
  portYIELD_FROM_ISR(w);
}

void inputsBegin() {
  pinMode(PIN_BTN_AC, INPUT_PULLUP);
  pinMode(PIN_BTN_WINDOW, INPUT_PULLUP);
  pinMode(PIN_BTN_ESTOP, INPUT_PULLUP);
  pinMode(PIN_LIMIT_OPEN, INPUT_PULLUP);
  pinMode(PIN_LIMIT_CLOSED, INPUT_PULLUP);
  pinMode(PIN_PRESENCE, INPUT); // PIR/LD2410 output drives the line

  attachInterrupt(digitalPinToInterrupt(PIN_BTN_AC), isrBtnAc, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_WINDOW), isrBtnWin, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_ESTOP), isrBtnEstop, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_PRESENCE), isrPresence, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_OPEN), isrLimit, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_LIMIT_CLOSED), isrLimit, FALLING);
}

static const uint8_t kPinOf[] = {PIN_BTN_AC, PIN_BTN_WINDOW, PIN_BTN_ESTOP, PIN_PRESENCE};

void inputTask(void *) {
  uint32_t lastEdge[4] = {0};
  uint8_t id;
  for (;;) {
    if (xQueueReceive(qIsr, &id, portMAX_DELAY) != pdTRUE || id > ISR_PRESENCE) continue;
    uint32_t now = millis();
    if (now - lastEdge[id] < DEBOUNCE_MS) continue; // contact bounce
    lastEdge[id] = now;

    if (id == ISR_PRESENCE) {
      Event e{EvType::PRESENCE, 1};
      xQueueSend(qEvents, &e, 0);
      continue;
    }
    // Button: confirm it is still pressed after the bounce window
    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS / 2));
    if (digitalRead(kPinOf[id]) != LOW) continue;
    Event e{id == ISR_BTN_AC ? EvType::BTN_AC : id == ISR_BTN_WINDOW ? EvType::BTN_WINDOW : EvType::BTN_ESTOP, 0};
    xQueueSend(qEvents, &e, 0);
  }
}
