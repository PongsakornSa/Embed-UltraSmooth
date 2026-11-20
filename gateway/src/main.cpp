// Gateway - Smart Classroom
// Creates shared state, queues and the FreeRTOS tasks. All work happens in tasks;
// loop() serves the local web dashboard independently of blocking cloud I/O.
#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "app.h"

Shared g;
SemaphoreHandle_t gMutex;
QueueHandle_t qEvents, qIsr, qPackets, qAlerts;
TaskHandle_t hWindowTask;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[gw] Smart Classroom gateway booting");

  Preferences prefs;
  prefs.begin("gw", false);
  g.stats.bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", g.stats.bootCount);
  prefs.end();

  if (!LittleFS.begin(true)) Serial.println("[gw] LittleFS mount failed");
#if !GATEWAY_WEB_ONLY
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
#endif

  gMutex = xSemaphoreCreateMutex();
  qEvents = xQueueCreate(16, sizeof(Event));
  qIsr = xQueueCreate(16, sizeof(uint8_t));
  qPackets = xQueueCreate(16, sizeof(SensorPacket) + 6);
  qAlerts = xQueueCreate(8, sizeof(AlertMsg));

  // Task watchdog: a stuck control/window task reboots the board (robustness)
  esp_task_wdt_init(10, true);

#if !GATEWAY_WEB_ONLY
  actuatorsBegin(); // first: put fan off and window in a known state
  inputsBegin();
  sensorsBegin();
  displayBegin();

  //                       function      name       stack  arg   prio  handle       core
  xTaskCreatePinnedToCore(windowTask,  "window",  3072, nullptr, 5, &hWindowTask, 1);
  xTaskCreatePinnedToCore(inputTask,   "input",   3072, nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 4096, nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(sensorTask,  "sensor",  4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(aiTask,      "ai",      4096, nullptr, 2, nullptr, 1);
#else
  Serial.printf("[gw] receiver + web server; MQ-135 source node=%s, local sensors/window actuator disabled\n", MQ135_NODE_ID);
#endif
#if GATEWAY_GAS_FAN_CONTROL
  pinMode(PIN_BTN_ESTOP, INPUT_PULLUP);
  fanBegin(); // model-controlled fan output; emergency-stop input is active-low
  Serial.printf("[gw] gas fan model enabled: GPIO%d relay active-%s, GPIO%d stop\n",
                PIN_FAN_RELAY, FAN_RELAY_ACTIVE_LOW ? "low" : "high", PIN_BTN_ESTOP);
#endif
  xTaskCreatePinnedToCore(nodeTask,    "node",    4096, nullptr, 3, nullptr, 1);
  // cloud task on core 0 next to the Wi-Fi stack; it also starts ESP-NOW
  xTaskCreatePinnedToCore(cloudTask,   "cloud",  12288, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(webDataTask, "webdata",12288, nullptr, 1, nullptr, 0);
}

void loop() {
  webPoll();
  delay(5);
}
