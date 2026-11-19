// display.cpp - 0.96" SSD1306 OLED at the front of the room (UX).
// Called only from sensorTask, so the I2C bus is never used by two tasks at once.
#include "app.h"
#if USE_OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);
static bool oledOk = false;

void displayBegin() {
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) { Serial.println("[oled] not found"); return; }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("Smart Classroom");
  oled.println("booting...");
  oled.display();
}

void displayUpdate() {
  if (!oledOk) return;
  char l1[24], l2[24], l3[24], l4[24], l5[24];
  {
    Lock l;
    const IndoorState &in = g.in;
    const NodeState *out = primaryOutdoorNode();
    snprintf(l1, sizeof(l1), "IN  %4.1fC %3.0f%% %s", in.tempC, in.rh, in.occupied ? "PPL" : "---");
    snprintf(l2, sizeof(l2), "CO2 %4u ppm", in.co2);
    if (out) snprintf(l3, sizeof(l3), "OUT PM2.5 %3.0f %s", out->last.pm25,
                      out->last.smoky ? "SMOKE" : (out->last.raining ? "RAIN" : ""));
    else snprintf(l3, sizeof(l3), "OUT node offline");
    snprintf(l4, sizeof(l4), "AC %s %3u%% (%s)", g.act.acOn ? "ON " : "OFF", g.act.fanPct, sourceName(g.act.acSource));
    if (g.act.estop) snprintf(l5, sizeof(l5), "!! EMERGENCY STOP !!");
    else snprintf(l5, sizeof(l5), "WIN %s %s AI%.0f%%", g.act.windowTarget == WindowTarget::OPEN ? "OPEN " : "CLOSE",
                  g.act.windowFault ? "FAULT" : (g.cloudOk ? "net" : "off"), g.ai.prob * 100);
  }
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println(l1);
  oled.println(l2);
  oled.println(l3);
  oled.println(l4);
  oled.println(l5);
  oled.display();
}
#else
void displayBegin() {}
void displayUpdate() {}
#endif
