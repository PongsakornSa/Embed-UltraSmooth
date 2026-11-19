// app.h - Shared state, events and module interfaces of the gateway.
//
// FreeRTOS tasks (E1) - priority reasoning:
//   windowTask  (5) safety: must stop the servo the instant a limit switch fires
//   inputTask   (4) turns ISR events into debounced button/limit/presence events (E2)
//   controlTask (4) decision logic; reacts to events immediately
//   nodeTask    (3) handles ESP-NOW packets, sends ACKs fast so nodes don't retry
//   sensorTask  (2) indoor sensors, slow (5 s period)
//   aiTask      (2) model inference once a minute
//   cloudTask   (1) Wi-Fi/MQTT/Telegram: slow, blocking network I/O -> lowest
// Communication: queues (events, packets, alerts), a mutex for shared state,
// task notifications from ISRs, and an event group for "cloud connected".
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include "protocol.h"
#include "config.h"

// ------------------------------------------------------------------ enums
enum class Source : uint8_t { NONE, BUTTON, DASHBOARD, AI, RULE, SAFETY };
enum class WindowTarget : uint8_t { CLOSED, OPEN };

inline const char *sourceName(Source s) {
  switch (s) {
    case Source::BUTTON: return "button";
    case Source::DASHBOARD: return "dashboard";
    case Source::AI: return "ai";
    case Source::RULE: return "rule";
    case Source::SAFETY: return "safety";
    default: return "none";
  }
}

// Events consumed by controlTask
enum class EvType : uint8_t {
  BTN_AC,        // manual toggle (physical button)
  BTN_WINDOW,
  BTN_ESTOP,
  CMD_AC,        // from dashboard: value 0=off 1=on 2=auto
  CMD_WINDOW,    // value 0=close 1=open 2=auto
  CMD_ESTOP_RESET,
  PRESENCE,      // value 1=motion
  WINDOW_FAULT,
};
struct Event {
  EvType type;
  int8_t value;
};

// Raw ISR events consumed by inputTask
enum IsrPin : uint8_t { ISR_BTN_AC, ISR_BTN_WINDOW, ISR_BTN_ESTOP, ISR_PRESENCE };

// ------------------------------------------------------------------ state
struct NodeState {
  char id[NODE_ID_LEN];
  SensorPacket last;
  uint32_t lastSeenMs;
  uint32_t received;   // unique packets
  uint32_t duplicates; // retransmissions we already had (ACK was lost)
  uint32_t gaps;       // seq numbers never received
  bool simulated;
};

struct IndoorState {
  float tempC = NAN, rh = NAN, lux = NAN;
  uint16_t co2 = 0;
  bool occupied = false;
  uint32_t lastMotionMs = 0;
  bool shtOk = false, scdOk = false, bhOk = false;
};

struct ActState {
  bool acOn = false;
  uint8_t fanPct = 0;
  Source acSource = Source::NONE;
  uint32_t acOnSinceMs = 0;
  WindowTarget windowTarget = WindowTarget::CLOSED;
  Source windowSource = Source::NONE;
  int windowAngle = WINDOW_ANGLE_CLOSED;
  bool windowMoving = false;
  bool windowFault = false;
  bool estop = false;
  uint32_t acManualUntil = 0;     // automation paused until this millis()
  uint32_t windowManualUntil = 0;
};

struct AiState {
  float prob = 0;
  bool valid = false;
  uint32_t lastRunMs = 0;
};

struct Stats {
  uint32_t acManualOnPresses = 0;  // labels for AI training
  uint32_t pressesSinceLog = 0;
  uint32_t published = 0, publishFailed = 0, buffered = 0, flushed = 0;
  uint32_t bootCount = 0;
};

struct Shared {
  NodeState nodes[MAX_NODES];
  int nodeCount = 0;
  IndoorState in;
  ActState act;
  AiState ai;
  Stats stats;
  bool cloudOk = false;
};

extern Shared g;
extern SemaphoreHandle_t gMutex;
extern QueueHandle_t qEvents;   // Event        -> controlTask
extern QueueHandle_t qIsr;      // IsrPin       -> inputTask
extern QueueHandle_t qPackets;  // RxPacket     -> nodeTask
extern QueueHandle_t qAlerts;   // AlertMsg     -> cloudTask
extern TaskHandle_t hWindowTask;

struct Lock { // RAII mutex guard: Lock l; ...access g...
  Lock() { xSemaphoreTake(gMutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(gMutex); }
};

struct AlertMsg { char text[160]; };
void raiseAlert(const char *key, const char *fmt, ...); // with per-key cooldown

// Helper: best real (non-simulated) outdoor node that is online, or nullptr.
// Call with gMutex held.
const NodeState *primaryOutdoorNode();

// ------------------------------------------------------------------ modules
void inputsBegin();      void inputTask(void *);
void actuatorsBegin();   void windowTask(void *);
void fanSet(uint8_t pct);
void sensorsBegin();     void sensorTask(void *);
void espnowBegin();      void nodeTask(void *);
void controlTask(void *);
void aiTask(void *);
void cloudTask(void *);
void displayBegin();     void displayUpdate();
