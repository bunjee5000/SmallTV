#include "DashboardWeatherValue.h"
#include <math.h>
#ifdef ARDUINO
#include <Arduino.h>
#define WEATHER_KEY(s) F(s)
#else
#define WEATHER_KEY(s) (s)
#endif
bool dashboardWeatherValue(JsonVariantConst root, int64_t now, DashboardWeatherValue& out) {
  JsonObjectConst current = root[WEATHER_KEY("current")];
  JsonVariantConst temp = current[WEATHER_KEY("temperature_2m")];
  JsonVariantConst stamp = current[WEATHER_KEY("time")];
  JsonVariantConst code = current[WEATHER_KEY("weather_code")];
  if (!temp.is<float>() || !isfinite(temp.as<float>()) || temp.as<float>() < -100 || temp.as<float>() > 70
      || !code.is<int>() || code.as<int>() < 0 || code.as<int>() > 99
      || !stamp.is<int64_t>() || stamp.as<int64_t>() <= 1609459200 || stamp.as<int64_t>() > now + 900)
    return false;
  out.temperature = temp.as<float>();
  out.observedUtc = (uint32_t)stamp.as<int64_t>();
  out.code = (uint8_t)code.as<int>();
  return true;
}
#undef WEATHER_KEY
