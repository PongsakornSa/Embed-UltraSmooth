#pragma once
#include <stddef.h>

// Allowlisted NETPIE fields; light/sound/flame remain empty until a node publishes them.
struct WebMetric { const char *id; const char *field; double min; double max; };
static const WebMetric WEB_METRICS[] = {
  {"mq135", "mq135_raw", 0, 4095},
  {"temperature", "temperature_c", -50, 150},
  {"light", "light_lux", 0, 1000000},
  {"sound", "sound_raw", 0, 4095},
  {"flame", "flame_raw", 0, 4095},
};
static constexpr size_t WEB_METRIC_COUNT = sizeof(WEB_METRICS) / sizeof(WEB_METRICS[0]);
