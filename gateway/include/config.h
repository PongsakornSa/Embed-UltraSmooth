// config.h - Gateway settings (pins, thresholds, timing). No secrets here:
// Wi-Fi / NETPIE / Telegram credentials live in secrets.h (git-ignored).
#pragma once

#define GW_HOSTNAME "smart-classroom-gw"

// ---- Pins (ESP32 DevKit v1) ----
#define PIN_I2C_SDA 21        // SCD40, SHT31, BH1750, OLED share this bus
#define PIN_I2C_SCL 22
#define PIN_PRESENCE 27       // PIR / LD2410 OUT (HIGH = person detected)
#define PIN_BTN_AC 32         // manual AC (fan) toggle, to GND, internal pull-up
#define PIN_BTN_WINDOW 33     // manual window toggle, to GND
#define PIN_BTN_ESTOP 25      // emergency stop, to GND
#define PIN_LIMIT_OPEN 26     // window fully-open limit switch, to GND
#define PIN_LIMIT_CLOSED 14   // window fully-closed limit switch, to GND
#define PIN_FAN_PWM 18        // MOSFET gate -> 12 V fan ("air conditioner")
#define PIN_SERVO 19          // window servo signal
#define PIN_LED_STATUS 2      // on-board LED: solid = cloud OK, blink = offline
#define PIN_SMOKE_AO 34       // MQ-2 analog out via 10k/20k divider (ADC1: works with Wi-Fi on)

// ---- Indoor smoke (MQ-2) ----
#define SMOKE_WARMUP_MS 60000     // heater needs ~1 min before readings mean anything
#define SMOKE_THRESHOLD 1800      // ADC raw above this = smoke (calibrate with incense)

// ---- LEDC (PWM) channels: different timers so frequencies do not clash ----
#define LEDC_CH_SERVO 0       // timer 0, 50 Hz
#define LEDC_CH_FAN 2         // timer 1, 25 kHz

// ---- Window servo ----
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2500
#define WINDOW_ANGLE_CLOSED 0
#define WINDOW_ANGLE_OPEN 90
#define WINDOW_STEP_DEG 2         // degrees per step
#define WINDOW_STEP_MS 25         // ms per step (smooth, slow motion)
#define WINDOW_MOVE_TIMEOUT_MS 6000 // safety: stop if a move takes longer

// ---- Fan ("AC") ----
#define FAN_MIN_PCT 40            // below this most 12 V fans stall
#define FAN_TEMP_LOW 26.0f        // fan speed ramps from FAN_MIN_PCT at this temp...
#define FAN_TEMP_HIGH 32.0f       // ...to 100 % at this temp

// ---- Decision thresholds ----
#define PM25_BAD 37.5f            // ug/m3, Thai PCD "starts to affect health"
#define PM25_GOOD 25.0f           // hysteresis: reopen only below this
#define CO2_HIGH 1000             // ppm, open window / ventilate
#define CO2_OK 800                // ppm, hysteresis
#define CO2_ALERT 1500            // ppm, Telegram alert
#define TEMP_RULE_AC_ON 30.0f     // fallback rule if AI unavailable
#define FREE_COOL_MIN_IN 27.0f    // open window for cooling if inside >= this...
#define FREE_COOL_DELTA 1.5f      // ...and outside is at least this much cooler
#define AI_ON_THRESHOLD 0.70f     // probability -> pre-cool (turn AC on)

// ---- Timing ----
#define MANUAL_HOLD_MS (30UL * 60 * 1000)   // manual command beats automation for 30 min
#define OCCUPANCY_HOLD_MS (5UL * 60 * 1000) // room "occupied" for 5 min after last motion
#define EMPTY_AC_OFF_MS (10UL * 60 * 1000)  // AC off after 10 min empty
#define NODE_OFFLINE_MS (2UL * 60 * 1000)   // node silent this long = offline
#define SENSOR_PERIOD_MS 5000
#define PUBLISH_PERIOD_MS 15000
#define AI_PERIOD_MS 60000
#define LOG_PERIOD_MS 60000                 // Serial LOG line (AI training data)
#define ALERT_COOLDOWN_MS (15UL * 60 * 1000)
#define DEBOUNCE_MS 60

// ---- Cloud (NETPIE 2020) ----
#define MQTT_HOST "mqtt.netpie.io"
#define MQTT_PORT_TLS 8883
#define TOPIC_SHADOW "@shadow/data/update"
#define TOPIC_CMD "@msg/cmd"
#define TOPIC_EVENT "@msg/event"
#define TOPIC_ALERT "@msg/alert"

// ---- Store-and-forward buffer (LittleFS) ----
#define BUFFER_FILE "/buffer.jsonl"
#define BUFFER_MAX_BYTES (256 * 1024)
#define BUFFER_FLUSH_PER_LOOP 20

#define MAX_NODES 6
#define USE_OLED 1
