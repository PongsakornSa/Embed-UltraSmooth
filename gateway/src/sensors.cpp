// sensors.cpp - Indoor sensors on the gateway: SCD40 (CO2), SHT31 (temp/RH),
// BH1750 (light), presence (PIR / LD2410), MQ-2 smoke (analog). All drivers are our own (common/include/drivers).
#include "app.h"
#include <Wire.h>
#include "drivers/scd4x.h"
#include "drivers/sht31.h"
#include "drivers/bh1750.h"

static Scd4x scd;
static Sht31 sht;
static Bh1750 bh;

void sensorsBegin() {
  bool okScd = scd.begin();
  bool okSht = sht.begin();
  bool okBh = bh.begin();
  Serial.printf("[sensor] SCD40=%d SHT31=%d BH1750=%d\n", okScd, okSht, okBh);
}

void sensorTask(void *) {
  int scdFails = 0;
  for (;;) {
    float t, h, lux, scdT, scdH;
    uint16_t co2;
    bool okSht = sht.read(t, h);
    bool okBh = bh.read(lux);
    bool okScd = false;
    if (scd.dataReady()) okScd = scd.read(co2, scdT, scdH);
    bool presenceHigh = digitalRead(PIN_PRESENCE) == HIGH;
    uint32_t smokeSum = 0; // MQ-2: average 8 samples to smooth ADC noise
    for (int i = 0; i < 8; i++) smokeSum += analogRead(PIN_SMOKE_AO);
    uint16_t smokeRaw = smokeSum / 8;
    bool smokeOk = millis() > SMOKE_WARMUP_MS;

    {
      Lock l;
      IndoorState &in = g.in;
      in.shtOk = okSht;
      if (okSht) { in.tempC = t; in.rh = h; } else { in.tempC = in.rh = NAN; }
      in.bhOk = okBh;
      in.lux = okBh ? lux : NAN;
      if (okScd) { in.co2 = co2; in.scdOk = true; scdFails = 0; }
      else if (++scdFails > 4) { in.scdOk = false; in.co2 = 0; } // >20 s without data
      in.smokeRaw = smokeRaw;
      in.smokeOk = smokeOk;
      in.smoky = smokeOk && smokeRaw > SMOKE_THRESHOLD;
      if (presenceHigh) in.lastMotionMs = millis();
      in.occupied = in.lastMotionMs != 0 && millis() - in.lastMotionMs < OCCUPANCY_HOLD_MS;
    }
    displayUpdate(); // same task as the sensors -> no I2C bus conflict
    vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}
