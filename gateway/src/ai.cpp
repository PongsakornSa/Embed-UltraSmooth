// ai.cpp - On-device AI (E7): predicts "will someone turn the AC on in the
// next 10 minutes?" from the current room state, so the gateway can pre-cool.
//
// Model: small neural network (MLP, 1 hidden layer, ReLU, sigmoid output)
// trained in Python (ai/train_model.py) on data logged by this gateway.
// Weights are exported to include/ac_model.h; inference is plain C math here.
//
// It also prints one LOG line per minute: that CSV stream IS the training
// data (ai/serial_logger.py saves it).
#include "app.h"
#include <time.h>
#include "ac_model.h"

static_assert(AC_N_FEATURES == 9, "feature list below must match ai/train_model.py");

// Temperature history (one sample per minute) for the 10-minute slope feature
static float tempHist[11];
static int histCount = 0;

static float predict(const float *x) {
  float hidden[AC_N_HIDDEN];
  for (int j = 0; j < AC_N_HIDDEN; j++) {
    float s = AC_B1[j];
    for (int i = 0; i < AC_N_FEATURES; i++)
      s += AC_W1[j][i] * ((x[i] - AC_FEAT_MEAN[i]) / AC_FEAT_STD[i]); // standardize
    hidden[j] = s > 0 ? s : 0; // ReLU
  }
  float out = AC_B2;
  for (int j = 0; j < AC_N_HIDDEN; j++) out += AC_W2[j] * hidden[j];
  return 1.0f / (1.0f + expf(-out)); // sigmoid -> probability
}

void aiTask(void *) {
  vTaskDelay(pdMS_TO_TICKS(10000));
  for (;;) {
    float x[AC_N_FEATURES];
    bool acOn, inputsOk;
    uint32_t presses;
    float outPm = NAN;
    uint8_t fanPct;
    int windowOpen;
    uint16_t co2;
    {
      Lock l;
      const IndoorState &in = g.in;
      const NodeState *out = primaryOutdoorNode();
      float outT = (out && (out->last.sensor_flags & SF_SHT)) ? out->last.temp_c : in.tempC; // impute
      if (out && (out->last.sensor_flags & SF_PMS)) outPm = out->last.pm25;

      time_t now = time(nullptr);
      struct tm lt;
      localtime_r(&now, &lt);
      float hour = (now > 1700000000) ? lt.tm_hour + lt.tm_min / 60.0f : 12.0f; // 12 if no NTP yet

      if (!isnan(in.tempC)) {
        if (histCount < 11) tempHist[histCount++] = in.tempC;
        else { memmove(tempHist, tempHist + 1, 10 * sizeof(float)); tempHist[10] = in.tempC; }
      }
      float slope = histCount >= 2 ? (tempHist[histCount - 1] - tempHist[0]) * 10.0f / (histCount - 1) : 0;

      x[0] = in.tempC;
      x[1] = in.rh;
      x[2] = in.co2;
      x[3] = in.occupied ? 1 : 0;
      x[4] = in.lux;
      x[5] = outT;
      x[6] = sinf(2 * PI * hour / 24);
      x[7] = cosf(2 * PI * hour / 24);
      x[8] = slope; // degC per 10 min

      inputsOk = in.shtOk && in.scdOk && in.bhOk;
      acOn = g.act.acOn;
      fanPct = g.act.fanPct;
      windowOpen = g.act.windowTarget == WindowTarget::OPEN;
      co2 = in.co2;
      presses = g.stats.pressesSinceLog;
      g.stats.pressesSinceLog = 0;
    }

    float p = (AC_MODEL_TRAINED && inputsOk) ? predict(x) : 0;
    {
      Lock l;
      g.ai.prob = p;
      g.ai.valid = AC_MODEL_TRAINED && inputsOk;
      g.ai.lastRunMs = millis();
    }

    // LOG,epoch,in_temp,in_rh,co2,occupied,lux,out_temp,hour_sin,hour_cos,slope,out_pm25,ac_on,fan,window,presses,ai_prob
    Serial.printf("LOG,%lu,%.2f,%.1f,%u,%.0f,%.0f,%.2f,%.4f,%.4f,%.3f,%.1f,%d,%u,%d,%lu,%.3f\n",
                  (unsigned long)time(nullptr), x[0], x[1], co2, x[3], x[4], x[5], x[6], x[7], x[8],
                  outPm, acOn, fanPct, windowOpen, (unsigned long)presses, p);

    vTaskDelay(pdMS_TO_TICKS(AI_PERIOD_MS));
  }
}
