#include "DashboardWeather.h"
#include "DashboardWeatherValue.h"
#include "Platform.h"
#include "Net.h"
#include <math.h>

static DashboardWeather g_weather = {};
static float g_lat = NAN, g_lon = NAN;
static uint32_t g_next = 0;
static bool g_scheduled = false;
const DashboardWeather& dashboardWeather() { return g_weather; }
void dashboardWeatherForce() { g_scheduled = false; }
void dashboardWeatherApply(const DashboardSettings& s) {
  if (g_lat == s.lat && g_lon == s.lon) return;
  g_lat = s.lat; g_lon = s.lon;
  // Never label cached weather from the previous coordinates as current here.
  g_weather.valid = false;
  g_weather.stage = WEATHER_IDLE;
  dashboardWeatherForce();
}
static uint32_t sampleHeap() {
  uint32_t heap = ESP.getFreeHeap();
  if (!g_weather.minSampled || heap < g_weather.minSampled) g_weather.minSampled = heap;
  return heap;
}
bool dashboardWeatherStale() {
  if (!g_weather.valid) return false;
  int64_t age = (int64_t)time(nullptr) - g_weather.observedUtc;
  return age > 5400 || age < -900 || millis() - g_weather.receivedMs > 5400000UL;
}

// ArduinoJson's reader interface, with a bounded input and total parse budget.
// The underlying Stream timeout bounds an individual stalled byte read.
class WeatherReader {
 public:
  WeatherReader(Stream& stream, uint16_t timeout)
      : stream_(stream), deadline_(millis() + timeout) { stream_.setTimeout(timeout); }
  int read() {
    if (read_ >= 2048 || (int32_t)(millis() - deadline_) >= 0) return -1;
    char c;
    if (!stream_.readBytes(&c, 1)) return -1;
    ++read_;
    if ((read_ & 63) == 0) yield();
    return (uint8_t)c;
  }
  size_t readBytes(char* buffer, size_t length) {
    size_t n = 0;
    while (n < length) { int c = read(); if (c < 0) break; buffer[n++] = (char)c; }
    return n;
  }
 private:
  Stream& stream_;
  uint32_t deadline_;
  uint16_t read_ = 0;
};

static bool __attribute__((noinline)) beginWeather(HTTPClient& http, WiFiClient& client,
                                                  const DashboardSettings& s) {
  // Public model data over the provider's verified HTTP endpoint: avoids a
  // second BearSSL footprint. No credentials, forecast arrays, or body String.
  char url[192];
  snprintf_P(url, sizeof(url), PSTR("http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code&timeformat=unixtime&forecast_days=1"),
             s.lat, s.lon);
  return http.begin(client, url); // HTTPClient copies the URL; release this stack buffer
}
static bool fetchWeather(const Settings& s) {
  WiFiClient client;
  HTTPClient http;
  http.useHTTP10(true); // tested with Host header: close-delimited, not chunked
  http.setReuse(false);
  http.setTimeout(s.httpTimeout);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!beginWeather(http, client, s.dashboard)) { g_weather.stage = WEATHER_CONNECT; return false; }
  http.addHeader(F("Accept"), F("application/json"));
  int code = http.GET();
  g_weather.http = code;
  sampleHeap();
  if (code != HTTP_CODE_OK) {
    g_weather.stage = code < 0 ? WEATHER_CONNECT : WEATHER_HTTP;
    http.end(); return false;
  }
  if (http.getSize() > 2048) {
    g_weather.stage = WEATHER_PARSE; http.end(); return false;
  }
  bool ok = false;
  {
    JsonDocument filter, doc;
    filter[F("current")][F("temperature_2m")] = true;
    filter[F("current")][F("weather_code")] = true;
    filter[F("current")][F("time")] = true;
    WeatherReader reader(http.getStream(), s.httpTimeout);
    DeserializationError error = deserializeJson(doc, reader,
        DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(4));
    sampleHeap();
    DashboardWeatherValue value;
    if (!error && dashboardWeatherValue(doc.as<JsonVariantConst>(), time(nullptr), value)) {
      g_weather.temperature = value.temperature;
      g_weather.code = value.code;
      g_weather.observedUtc = value.observedUtc;
      g_weather.receivedMs = millis();
      g_weather.valid = true;
      ok = true;
    }
  } // release both JSON documents before stopping the socket
  g_weather.stage = ok ? WEATHER_OK : WEATHER_PARSE;
  http.end();
  return ok;
}

bool dashboardWeatherService(const Settings& s) {
  dashboardWeatherApply(s.dashboard);
  if (!s.dashboard.enabled || !s.dashboard.weather || !netConnected() || !platformTimeValid()) return false;
  if (g_scheduled && (int32_t)(millis() - g_next) < 0) return false;
  g_scheduled = true;
  ++g_weather.attempts;
  g_weather.http = 0;
  sampleHeap();
  bool ok = false;
  if (ESP.getFreeHeap() < 5000 || platformMaxFreeBlock() < 2048) g_weather.stage = WEATHER_LOW_HEAP;
  else ok = fetchWeather(s);
  g_weather.heapClosed = sampleHeap();
  g_weather.blockClosed = platformMaxFreeBlock();
  uint32_t interval = ok ? 1500000UL : (g_weather.stage == WEATHER_LOW_HEAP ? 60000UL : 300000UL);
  g_next = millis() + interval;
  return true;
}
