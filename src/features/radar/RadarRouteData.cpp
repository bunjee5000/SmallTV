#include "RadarRouteData.h"
#include <string.h>
#ifdef ARDUINO
#include <Arduino.h>
#define ROUTE_KEY(s) F(s)
#else
#define ROUTE_KEY(s) (s)
#endif

static bool letter(char c) { return c >= 'A' && c <= 'Z'; }
static bool digit(char c) { return c >= '0' && c <= '9'; }
static bool code(const char* s, size_t low, size_t high, bool numbers) {
  if (!s) return false;
  size_t n = strlen(s);
  if (n < low || n > high) return false;
  for (size_t i=0; i<n; ++i) if (!letter(s[i]) && !(numbers && digit(s[i]))) return false;
  return true;
}
bool radarRouteEligible(const char* callsign) {
  if (!code(callsign, 3, 8, true)) return false;
  return (letter(callsign[0]) && letter(callsign[1]) && letter(callsign[2]) && digit(callsign[3]))
      || (digit(callsign[2]) && (letter(callsign[0]) || letter(callsign[1])));
}
static bool airport(JsonObjectConst object, char out[5]) {
  const char* value = object[ROUTE_KEY("iata_code")];
  if (!code(value, 3, 3, false)) {
    value = object[ROUTE_KEY("icao_code")];
    if (!code(value, 4, 4, true)) return false;
  }
  strcpy(out, value);
  return true;
}
bool radarDecodeRoute(JsonVariantConst root, const char* callsign, RadarRouteInfo& out) {
  JsonObjectConst route = root[ROUTE_KEY("response")][ROUTE_KEY("flightroute")];
  if (!radarRouteEligible(callsign) || route.isNull()) return false;
  bool match = false;
  for (const char* value : {route[ROUTE_KEY("callsign")].as<const char*>(),
                           route[ROUTE_KEY("callsign_icao")].as<const char*>(),
                           route[ROUTE_KEY("callsign_iata")].as<const char*>()})
    if (value && !strcmp(value, callsign)) match = true;
  if (!match) return false;
  RadarRouteInfo result = {};
  if (!airport(route[ROUTE_KEY("origin")], result.origin)
      || !airport(route[ROUTE_KEY("destination")], result.destination)) return false;
  const char* flight = route[ROUTE_KEY("callsign_iata")];
  if (code(flight, 3, 8, true) && digit(flight[2])) strcpy(result.flight, flight);
  // An absent IATA mapping retains the original ADS-B callsign.
  out = result;
  return true;
}
void radarFlightLabel(const char* callsign, const RadarRouteInfo* route, char out[10]) {
  const char* value = route && route->flight[0] ? route->flight : callsign;
  size_t n = strlen(value);
  if (n > 8) n = 8;
  if (route && route->flight[0]) {
    memcpy(out, value, 2); out[2] = ' '; memcpy(out+3, value+2, n-2); out[n+1] = 0;
  } else { memcpy(out, value, n); out[n] = 0; }
}
bool RadarRouteCache::due(const char* callsign, uint32_t now) const {
  for (const auto& entry : entries_) if (!strcmp(entry.callsign, callsign))
    return (int32_t)(now-entry.retryAt) >= 0;
  return true;
}
const RadarRouteInfo* RadarRouteCache::lookup(const char* callsign, uint32_t now) const {
  for (const auto& entry : entries_) if (!strcmp(entry.callsign, callsign))
    return entry.info.origin[0] && (int32_t)(now-entry.retryAt) < 0 ? &entry.info : nullptr;
  return nullptr;
}
void RadarRouteCache::remember(const char* callsign, const RadarRouteInfo* value, uint32_t now,
                               uint32_t ttl, const char* const* active, uint8_t count) {
  RadarRouteEntry* slot = nullptr;
  for (auto& entry : entries_) {
    if (!strcmp(entry.callsign, callsign)) { slot = &entry; break; }
    bool visible = false;
    for (uint8_t i=0; i<count; ++i) if (!strcmp(entry.callsign, active[i])) { visible = true; break; }
    if (!slot && (!entry.callsign[0] || !visible)) slot = &entry;
  }
  if (!slot) return;
  if (strcmp(slot->callsign, callsign)) *slot = RadarRouteEntry();
  size_t n = strlen(callsign); if (n > 8) n = 8;
  memcpy(slot->callsign, callsign, n); slot->callsign[n] = 0;
  if (value) slot->info = *value; // transient failures retain the same flight's last route
  slot->retryAt = now + ttl;
}
#undef ROUTE_KEY
