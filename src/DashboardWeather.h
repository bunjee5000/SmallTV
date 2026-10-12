#pragma once
#include "Settings.h"

enum DashboardWeatherStage : uint8_t {
  WEATHER_IDLE, WEATHER_OK, WEATHER_LOW_HEAP, WEATHER_CONNECT, WEATHER_HTTP, WEATHER_PARSE
};
struct DashboardWeather {
  float temperature;
  uint32_t observedUtc, receivedMs, attempts;
  uint32_t heapClosed, blockClosed, minSampled;
  int16_t http;
  uint8_t code, stage;
  bool valid;
};
const DashboardWeather& dashboardWeather();
void dashboardWeatherApply(const DashboardSettings& s);
void dashboardWeatherForce();
bool dashboardWeatherService(const Settings& s); // true if an attempt ran
bool dashboardWeatherStale();
