#pragma once
#include <ArduinoJson.h>
#include <stdint.h>
struct DashboardWeatherValue { float temperature; uint32_t observedUtc; uint8_t code; };
// Validates the API payload transactionally; out is untouched on failure.
bool dashboardWeatherValue(JsonVariantConst root, int64_t now, DashboardWeatherValue& out);
