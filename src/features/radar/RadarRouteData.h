#pragma once
#include <stdint.h>
#include <ArduinoJson.h>
#include "config.h"

struct RadarRouteInfo { char flight[9], origin[5], destination[5]; };
struct RadarRouteEntry { uint32_t retryAt; char callsign[9]; RadarRouteInfo info; };
static_assert(sizeof(RadarRouteEntry) == 32, "Keep the route cache compact");

bool radarRouteEligible(const char* callsign);
bool radarDecodeRoute(JsonVariantConst root, const char* callsign, RadarRouteInfo& out);
void radarFlightLabel(const char* callsign, const RadarRouteInfo* route, char out[10]);

class RadarRouteCache {
 public:
  bool due(const char* callsign, uint32_t now) const;
  const RadarRouteInfo* lookup(const char* callsign, uint32_t now) const;
  // active[] protects cache entries for aircraft currently on the scope.
  void remember(const char* callsign, const RadarRouteInfo* value, uint32_t now,
                uint32_t ttl, const char* const* active, uint8_t count);
 private:
  RadarRouteEntry entries_[MAX_AIRCRAFT] = {};
};
