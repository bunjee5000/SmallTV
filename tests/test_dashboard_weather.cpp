#include "DashboardWeatherValue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool parse(const char* body, DashboardWeatherValue& value) {
  JsonDocument doc;
  auto error = deserializeJson(doc, body);
  return !error && dashboardWeatherValue(doc.as<JsonVariantConst>(), 1791829800, value);
}
int main() {
  DashboardWeatherValue value={-7.5f,123,45};
  const char* invalid[]={
    "{}", "{\"current\":null}", "{\"error\":true}",
    "{\"current\":{\"temperature_2m\":null,\"weather_code\":0,\"time\":1791829800}}",
    "{\"current\":{\"temperature_2m\":12,\"weather_code\":null,\"time\":1791829800}}",
    "{\"current\":{\"temperature_2m\":12,\"weather_code\":1.5,\"time\":1791829800}}",
    "{\"current\":{\"temperature_2m\":12,\"weather_code\":0,\"time\":null}}",
    "{\"current\":{\"temperature_2m\":99,\"weather_code\":0,\"time\":1791829800}}",
    "{\"current\":{\"temperature_2m\":12,\"weather_code\":0,\"time\":1791839800}}",
    "{\"current\":{\"temperature_2m\":12,\"weather_code\":0,\"time\":1791829800}"
  };
  for(const char* body:invalid) {
    assert(!parse(body,value));
    assert(value.temperature==-7.5f && value.observedUtc==123 && value.code==45);
  }
  assert(parse("{\"current\":{\"time\":1791829800,\"interval\":900,\"temperature_2m\":12.5,\"weather_code\":1},\"latitude\":52.37}",value));
  assert(value.temperature==12.5f && value.observedUtc==1791829800 && value.code==1);
  // Old provider data remains valid cached information; the renderer marks it stale.
  assert(parse("{\"current\":{\"temperature_2m\":-12,\"weather_code\":71,\"time\":1791810000}}",value));
  assert(value.temperature==-12 && value.code==71);
  puts("Weather validation: good/old data accepted; missing/null/invalid/truncated data rejected without replacing cache.");
}
