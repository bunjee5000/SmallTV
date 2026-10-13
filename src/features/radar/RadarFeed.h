#pragma once
#include "RadarData.h"
#include <ArduinoJson.h>
#include <math.h>

struct RadarFeedSettings { float lat, lon; uint16_t minAltFt; };
struct RadarFeedResult { uint8_t count = 0; uint16_t seen = 0; bool found = false; };

// One-byte lookahead shared by the envelope parser and ArduinoJson. Input's
// read() must wait for a byte (or return -1 on timeout/end of stream).
template<class Input> class RadarJsonInput {
 public:
  explicit RadarJsonInput(Input& input) : input_(input) {}
  int read() { int c = saved_ == -2 ? input_.read() : saved_; saved_ = -2; return c; }
  int peek() { if (saved_ == -2) saved_ = input_.read(); return saved_; }
  int next() { int c; do { c = peek(); if (c == ' ' || c == '\r' || c == '\n' || c == '\t') read(); else return c; } while (true); }
  size_t readBytes(char* out, size_t length) {
    size_t n = 0; int c;
    while (n < length && (c = read()) >= 0) out[n++] = (char)c;
    return n;
  }
 private:
  Input& input_;
  int saved_ = -2;
};

// Present scalar numbers as a complete input to ArduinoJson without consuming
// the envelope's delimiter. This also validates ignored numeric metadata.
template<class Input> class RadarNumberInput {
 public:
  explicit RadarNumberInput(RadarJsonInput<Input>& json) : json_(json) {}
  int read() {
    int c = json_.peek();
    if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\r' || c == '\n') return -1;
    return json_.read();
  }
  size_t readBytes(char* out, size_t length) {
    size_t n=0; int c;
    while (n<length && (c=read())>=0) out[n++]=(char)c;
    return n;
  }
 private:
  RadarJsonInput<Input>& json_;
};

template<class Input, class Filter>
bool radarFeedValue(JsonDocument& doc, RadarJsonInput<Input>& json, Filter filter) {
  int c = json.next();
  bool number = (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
  if (number) {
    RadarNumberInput<Input> value(json);
    return !deserializeJson(doc, value);
  }
  if (!number && c != '{' && c != '[' && c != '"' && c != '\'' && c != 't' && c != 'f' && c != 'n') return false;
  if (deserializeJson(doc, json, DeserializationOption::Filter(filter),
                      DeserializationOption::NestingLimit(10))) return false;
  return true;
}

inline void radarInsertNearest(Aircraft* pending, uint8_t& count, uint8_t capacity, const Aircraft& a) {
  if (count == capacity && a.distKm >= pending[count-1].distKm) return;
  uint8_t i = count < capacity ? count : capacity-1;
  while (i && pending[i-1].distKm > a.distKm) { pending[i] = pending[i-1]; --i; }
  pending[i] = a;
  if (count < capacity) ++count;
}

inline void radarFeedAircraft(JsonObjectConst a, const RadarFeedSettings& s,
                              Aircraft* pending, uint8_t capacity, RadarFeedResult& result) {
  if (!a[F("lat")].is<float>() || !a[F("lon")].is<float>()) return;
  float lat = a[F("lat")], lon = a[F("lon")];
  if (!isfinite(lat) || !isfinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) return;
  Aircraft t = {};
  t.track = a[F("track")].is<float>() ? a[F("track")].as<float>() : NAN;
  t.gs = a[F("gs")].is<float>() ? a[F("gs")].as<float>() : NAN;
  t.altFt = a[F("alt_baro")].is<int>() ? a[F("alt_baro")].as<int>() : 0;
  if (s.minAltFt && t.altFt < s.minAltFt) return;
  const char* flight = a[F("flight")] | "";
  while (*flight == ' ' || *flight == '\t') ++flight;
  size_t length = strlen(flight);
  while (length && (flight[length-1] == ' ' || flight[length-1] == '\t')) --length;
  if (!length) { flight = a[F("hex")] | ""; length = strlen(flight); }
  if (length >= sizeof(t.callsign)) length = sizeof(t.callsign)-1;
  memcpy(t.callsign, flight, length);
  float north = (lat-s.lat)*111.0f;
  float east = (lon-s.lon)*111.0f*cosf(s.lat*(float)PI/180.0f);
  t.distKm = sqrtf(north*north + east*east);
  t.bearingDeg = atan2f(east, north)*180.0f/(float)PI;
  if (t.bearingDeg < 0) t.bearingDeg += 360;
  radarInsertNearest(pending, result.count, capacity, t);
}

// Validate the complete top-level JSON object, retaining only one aircraft
// document at a time. The caller publishes pending only on success, so a
// truncated response never replaces the last good aircraft snapshot.
template<class Input, class Sample>
bool radarParseFeed(Input& input, const RadarFeedSettings& s, Aircraft* pending,
                    uint8_t capacity, RadarFeedResult& result, Sample sample,
                    ArduinoJson::Allocator* allocator = ArduinoJson::detail::DefaultAllocator::instance()) {
  if (!capacity || !pending) return false;
  result = RadarFeedResult();
  RadarJsonInput<Input> json(input);
  JsonDocument filter(allocator), doc(allocator), key(allocator);
  filter[F("lat")] = true; filter[F("lon")] = true;
  filter[F("track")] = true; filter[F("gs")] = true;
  filter[F("flight")] = true; filter[F("hex")] = true;
  filter[F("alt_baro")] = true;
  if (filter.overflowed() || json.next() != '{') return false;
  json.read();
  if (json.next() == '}') { json.read(); return true; }
  while (true) {
    if (json.next() != '"' || deserializeJson(key, json) || !key.is<const char*>()) return false;
    bool aircraft = strcmp(key.as<const char*>(), "ac") == 0;
    if (json.next() != ':') return false;
    json.read();
    if (aircraft) {
      if (result.found || json.next() != '[') return false;
      result.found = true;
      json.read();
      if (json.next() != ']') {
        while (true) {
          if (!radarFeedValue(doc, json, filter)) return false;
          sample();
          if (result.seen != UINT16_MAX) ++result.seen;
          radarFeedAircraft(doc.as<JsonObjectConst>(), s, pending, capacity, result);
          int separator = json.next();
          if (separator == ']') break;
          if (separator != ',') return false;
          json.read();
        }
      }
      json.read(); // closing array
    } else {
      if (!radarFeedValue(doc, json, JsonVariantConst())) return false;
    }
    int separator = json.next();
    if (separator == '}') { json.read(); return true; }
    if (separator != ',') return false;
    json.read();
  }
}
