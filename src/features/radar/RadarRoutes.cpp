#include "RadarRoutes.h"
#include "RadarClient.h"
#include "Platform.h"
#include "Net.h"
#include <memory>

static RadarRouteCache g_cache;
static RadarRouteStatus g_status = {};
static TlsSession g_session; // separate host; never overwrite the ADS-B TLS session
static uint32_t g_next = 0;
static bool g_scheduled = false;
const RadarRouteStatus& radarRouteStatus() { return g_status; }
const RadarRouteInfo* radarRouteFor(const char* callsign) { return g_cache.lookup(callsign, millis()); }
static void sample() {
  uint32_t heap = ESP.getFreeHeap();
  if (!g_status.minSampled || heap < g_status.minSampled) g_status.minSampled = heap;
}

class RouteReader {
 public:
  RouteReader(Stream& stream, uint16_t timeout) : stream_(stream), deadline_(millis()+timeout) {}
  int read() {
    int32_t remaining = (int32_t)(deadline_-millis());
    if (remaining <= 0 || bytes_ >= 4096) return -1;
    stream_.setTimeout((uint32_t)remaining);
    char c;
    if (!stream_.readBytes(&c, 1)) return -1;
    if ((++bytes_ & 63) == 0) yield();
    return (uint8_t)c;
  }
  size_t readBytes(char* out, size_t length) {
    size_t n=0; int c;
    while (n<length && (c=read())>=0) out[n++]=(char)c;
    return n;
  }
 private:
  Stream& stream_;
  uint32_t deadline_;
  uint16_t bytes_ = 0;
};

static bool fetch(const char* callsign, uint16_t timeout, RadarRouteInfo& value) {
  // api.adsbdb.com was verified to negotiate TLS 1.2 MFLN at 512 bytes.
  // Keep this optional request small; no automatic 16 KB fallback allocation.
  std::unique_ptr<SecureClient> client(platformMakeSecureClient(512, &g_session));
  HTTPClient http;
  char url[80];
  snprintf_P(url, sizeof(url), PSTR("https://api.adsbdb.com/v0/callsign/%s"), callsign);
  http.useHTTP10(true);
  http.setReuse(false);
  http.setTimeout(timeout);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(*client, url)) return false;
  http.addHeader(F("Accept"), F("application/json"));
  http.setUserAgent(F(ADSB_USER_AGENT));
  g_status.http = http.GET();
  sample();
  if (g_status.http != HTTP_CODE_OK) { http.end(); return false; }
  bool ok = false;
  if (http.getSize() <= 4096) {
    JsonDocument filter, doc;
    JsonObject route = filter[F("response")][F("flightroute")].to<JsonObject>();
    route[F("callsign")] = true; route[F("callsign_icao")] = true; route[F("callsign_iata")] = true;
    route[F("origin")][F("iata_code")] = true; route[F("origin")][F("icao_code")] = true;
    route[F("destination")][F("iata_code")] = true; route[F("destination")][F("icao_code")] = true;
    if (!filter.overflowed()) {
      RouteReader reader(http.getStream(), timeout);
      auto error = deserializeJson(doc, reader, DeserializationOption::Filter(filter),
                                   DeserializationOption::NestingLimit(5));
      sample();
      ok = !error && radarDecodeRoute(doc.as<JsonVariantConst>(), callsign, value);
    }
  }
  http.end();
  return ok;
}

bool radarRoutesService(const Settings& s) {
  if (!s.radar.showRoutes || !s.radar.showLabels || radarError() || !netConnected()) return false;
  if (s.radar.lat == 0.0f && s.radar.lon == 0.0f) return false;
  uint32_t now = millis();
  if (g_scheduled && (int32_t)(now-g_next) < 0) return false;
  // Aircraft polling always gets the next loop iteration when already due.
  // Short refresh periods leave fewer opportunities for enrichment.
  if (radarPollInMs() < 2500) return false;
  const char* active[MAX_AIRCRAFT];
  uint8_t count = radarCount();
  for (uint8_t i=0; i<count; ++i) active[i]=aircraftAt(i).callsign;
  const char* selected = nullptr;
  for (uint8_t i=0; i<count; ++i) {
    if (aircraftAt(i).distKm <= s.radar.rangeKm && radarRouteEligible(active[i]) && g_cache.due(active[i],now)) {
      selected=active[i]; break;
    }
  }
  if (!selected) return false;
  g_scheduled = true;
  g_next = now+15000UL; // at most four lookups per minute, including unknown callsigns
  if (ESP.getFreeHeap() < 22000 || platformMaxFreeBlock() < 16000) {
    g_status.stage = ROUTE_LOW_HEAP;
    return false;
  }
  ++g_status.attempts;
  g_status.http = 0;
  sample();
  RadarRouteInfo value = {};
  uint16_t timeout = s.httpTimeout < 1500 ? s.httpTimeout : 1500;
  bool ok = fetch(selected, timeout, value);
  sample();
  g_status.heapClosed = ESP.getFreeHeap();
  g_status.blockClosed = platformMaxFreeBlock();
  if (ok) {
    g_cache.remember(selected, &value, millis(), 21600000UL, active, count); // six hours
    g_status.stage = ROUTE_OK;
  } else if (g_status.http == HTTP_CODE_NOT_FOUND) {
    g_cache.remember(selected, &value, millis(), 3600000UL, active, count); // unknown for one hour
    g_status.stage = ROUTE_UNKNOWN;
  } else {
    g_cache.remember(selected, nullptr, millis(), 900000UL, active, count); // retry after 15 minutes
    g_status.stage = ROUTE_ERROR;
    g_next = millis()+300000UL; // provider/connection failure: back off globally for five minutes
  }
  return true;
}
