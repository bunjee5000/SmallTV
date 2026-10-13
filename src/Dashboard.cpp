#include "Dashboard.h"
#include "DashboardMath.h"
#include "DashboardWeather.h"
#include "Gfx.h"
#include <Arduino_GFX_Library.h>
#include <math.h>

static DashboardDay g_days[3];
static int32_t g_day = INT32_MIN;
// Built-in glcdfont uses CP437 bytes directly (not UTF-8): ä=132, ö=148, °=248.
// Two size-2 lines give phase names the same weight as the temperature.
static const char moonNames[][2][9] PROGMEM = {
  {"Neumond", ""}, {"Zun.", "Sichel"}, {"Erstes", "Viertel"}, {"Zun.", "Mond"},
  {"Vollmond", ""}, {"Abn.", "Mond"}, {"Letztes", "Viertel"}, {"Abn.", "Sichel"}
};
static const char prayerNames[][8] PROGMEM = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
static const char weekdays[][3] PROGMEM = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
static const char months[][5] PROGMEM = {"Jan", "Feb", "M\x84rz", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};

void dashboardInvalidate(const Settings& s) {
  g_day = INT32_MIN;
  dashboardWeatherApply(s.dashboard);
}
// Center using the renderer's actual bounds, including glyph offsets. Flash
// strings stay in flash; measurement and printing allocate no temporary String.
static void centered(Arduino_GFX* d, int cx, int y, uint8_t size, uint16_t color,
                     const char* value, bool inFlash = false) {
  d->setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  if (inFlash) d->getTextBounds(FPSTR(value), 0, 0, &x1, &y1, &w, &h);
  else d->getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
  d->setTextColor(color); d->setCursor(cx-w/2-x1, y-y1);
  if (inFlash) d->print(FPSTR(value));
  else d->print(value);
}
static void centerFlash(Arduino_GFX* d, int y, uint8_t size, uint16_t color, const char* value) {
  centered(d, 120, y, size, color, value, true);
}
static void cloud(Arduino_GFX* d, int x, int y, uint16_t color) {
  d->fillCircle(x-6, y, 6, color); d->fillCircle(x+1, y-4, 8, color);
  d->fillCircle(x+9, y, 5, color); d->fillRect(x-7, y, 17, 5, color);
}
static void sun(Arduino_GFX* d, int x, int y, uint16_t color) {
  d->fillCircle(x, y, 5, color);
  for (int a = 0; a < 8; ++a) {
    float angle = a * (float)PI / 4;
    d->drawLine(x + lroundf(cosf(angle)*8), y + lroundf(sinf(angle)*8),
                x + lroundf(cosf(angle)*11), y + lroundf(sinf(angle)*11), color);
  }
}
// WMO groups. Unknown codes get a neutral icon/label instead of clear skies.
static int weatherKind(int code) {
  if (code == 0) return 0;
  if (code == 1 || code == 2) return 1;
  if (code == 3) return 2;
  if (code == 45 || code == 48) return 5;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return 3;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return 4;
  if (code == 95 || code == 96 || code == 99) return 6;
  return 7;
}
static const char weatherNames[][14] PROGMEM = {
  "Klar", "Leicht bew.", "Bew\x94lkt", "Regen", "Schnee", "Nebel", "Gewitter", "Unbekannt"
};
static void weatherIcon(Arduino_GFX* d, int kind) {
  const int x = 60, y = 110;
  uint16_t accent = gfxTint(0x5E9F);
  if (kind == 0) { sun(d, x, y, accent); return; }
  if (kind == 1) sun(d, x-5, y-5, accent);
  if (kind <= 4 || kind == 6) cloud(d, x, y, C_GRAY);
  if (kind == 3) for (int i = -1; i <= 1; ++i) d->drawLine(x+i*7, y+9, x+i*7-2, y+14, accent);
  if (kind == 4) for (int i = -1; i <= 1; ++i) {
    d->drawFastHLine(x+i*7-2, y+11, 5, C_WHITE); d->drawFastVLine(x+i*7, y+9, 5, C_WHITE);
  }
  if (kind == 5) for (int i = 0; i < 3; ++i) d->drawFastHLine(x-12+i*2, y-5+i*6, 24-i*4, C_GRAY);
  if (kind == 6) {
    d->drawLine(x+2, y+6, x-3, y+11, accent); d->drawLine(x-3, y+11, x+3, y+11, accent);
    d->drawLine(x+3, y+11, x-2, y+16, accent);
  }
  if (kind == 7) d->drawCircle(x, y, 10, C_DGRAY);
}
static void moonIcon(Arduino_GFX* d, float cycle) {
  const int cx = 180, cy = 110, radius = 14;
  // The unlit hemisphere must look dark, not like a second illuminated disk.
  d->fillCircle(cx, cy, radius, gfxTint(0x1082));
  float terminator = cosf(cycle * 2 * (float)PI);
  if (fabsf(terminator) < 0.00001f) terminator = 0; // stable exact quarters
  for (int y = -radius+1; y < radius; ++y) {
    int half = (int)sqrtf((float)(radius*radius - y*y));
    // Sample pixel centers. Inclusive endpoints give mirror-symmetric waxing
    // and waning crescents, zero light at new and a filled interior at full.
    int edge = (int)ceilf(terminator * (half + 0.5f));
    if (edge < -half) edge = -half;
    int left = cycle < 0.5f ? edge : -half;
    int right = cycle < 0.5f ? half : -edge;
    if (right >= left) d->drawFastHLine(cx+left, cy+y, right-left+1, C_WHITE);
  }
  d->drawCircle(cx, cy, radius, C_DGRAY);
}
static void clockText(int64_t utc, char out[6]) {
  time_t stamp = (time_t)utc;
  struct tm local;
  localtime_r(&stamp, &local); // TZ/DST at this event, not the current offset
  snprintf_P(out, 6, PSTR("%02d:%02d"), local.tm_hour, local.tm_min);
}
static void sunText(int32_t day, int16_t minutes, char out[6]) {
  if (minutes == DASH_NO_TIME) { strcpy_P(out, PSTR("--:--")); return; }
  clockText((int64_t)day * 86400 + minutes * 60, out);
}
static void updateDay(const DashboardSettings& s, int32_t day) {
  if (g_day == day) return;
  double fajr = s.fajrAngle, isha = s.ishaAngle;
  switch (s.method) {
    case DASH_MWL: fajr = 18; isha = 17; break;
    case DASH_ISNA: fajr = isha = 15; break;
    case DASH_EGYPT: fajr = 19.5; isha = 17.5; break;
    case DASH_KARACHI: fajr = isha = 18; break;
  }
  for (int i = 0; i < 3; ++i) {
    g_days[i] = dashboardPrayerDay(day+i-1, s.lat, s.lon, fajr, isha, s.asr ? 2 : 1, s.highLatitude);
    yield();
  }
  g_day = day;
}

void dashboardRender(const Settings& settings, const struct tm& now) {
  Arduino_GFX* d = gfxDev();
  if (!d) return;
  const DashboardSettings& s = settings.dashboard;
  int32_t day = dashboardCivilDay(now.tm_year+1900, now.tm_mon+1, now.tm_mday);
  updateDay(s, day);
  int64_t epoch = time(nullptr);
  uint16_t accent = gfxTint(0x5E9F); // one soft cyan accent
  char buf[40], name[8]; // fixed stack buffers; no Strings or heap allocations
  d->setTextWrap(false);
  d->fillScreen(C_BLACK);
  snprintf_P(buf, sizeof(buf), PSTR("%02d:%02d"), now.tm_hour, now.tm_min);
  centered(d, 120, 10, 5, C_WHITE, buf);
  // Avoid strftime: linking its locale machinery costs static RAM on ESP8266.
  char weekday[3], month[5];
  memcpy_P(weekday, weekdays[now.tm_wday], sizeof(weekday));
  memcpy_P(month, months[now.tm_mon], sizeof(month));
  snprintf_P(buf, sizeof(buf), PSTR("%s %02d. %s %d"), weekday, now.tm_mday, month, now.tm_year+1900);
  centered(d, 120, 58, 2, C_GRAY, buf);
  d->drawFastHLine(8, 80, 224, C_DGRAY);
  centered(d, 60, 86, 1, C_GRAY, PSTR("Wetter"), true);
  centered(d, 180, 86, 1, C_GRAY, PSTR("Mond"), true);

  const DashboardWeather& weather = dashboardWeather();
  if (s.weather && weather.valid) {
    int kind = weatherKind(weather.code);
    weatherIcon(d, kind);
    snprintf_P(buf, sizeof(buf), PSTR("%ld \xF8" "C"), lroundf(weather.temperature));
    centered(d, 60, 130, 2, C_WHITE, buf);
    centered(d, 60, 150, 1, C_GRAY, weatherNames[kind], true);
    if (dashboardWeatherStale()) centered(d, 60, 162, 1, accent, PSTR("veralt."), true);
    else if (weather.stage != WEATHER_OK) centered(d, 60, 162, 1, C_GRAY, PSTR("Neuversuch"), true);
  } else {
    weatherIcon(d, 7);
    centered(d, 60, 130, 2, C_WHITE, PSTR("-- \xF8" "C"), true);
    centered(d, 60, 150, 1, C_GRAY, s.weather ? PSTR("Wartezeit") : PSTR("Wetter aus"), true);
  }
  DashboardMoon moon = dashboardMoon(epoch);
  moonIcon(d, moon.cycle);
  for (int line = 0; line < 2; ++line) {
    const char* phase = moonNames[moon.index][line];
    if (strlen_P(phase)) centered(d, 180, 130+line*20, 2, C_WHITE, phase, true);
  }
  d->drawFastHLine(8, 172, 224, C_DGRAY);
  centerFlash(d, 178, 1, C_GRAY, PSTR("N\x84" "chstes Gebet"));

  int64_t next;
  uint8_t adjusted;
  int prayer = dashboardNextPrayer(g_days, day, epoch, &next, &adjusted);
  char hhmm[6];
  if (prayer >= 0) {
    memcpy_P(name, prayerNames[prayer], sizeof(prayerNames[0]));
    clockText(next, hhmm);
    snprintf_P(buf, sizeof(buf), PSTR("%s %s"), name, hhmm);
    centered(d, 120, 190, 2, C_WHITE, buf);
    if (s.countdown) {
      unsigned minutes = (unsigned)((next - epoch + 59) / 60);
      snprintf_P(buf, sizeof(buf), PSTR("in %u Std. %u Min."),
                 minutes/60, minutes%60);
      centered(d, 120, 208, 2, accent, buf);
    }
  } else {
    centerFlash(d, 190, 2, C_WHITE, PSTR("Keine Gebetszeit"));
    centerFlash(d, 212, 1, accent, PSTR("Ort / Methode pr\x81" "fen"));
  }
  char rise[6], set[6];
  sunText(day, g_days[1].sunrise, rise); sunText(day, g_days[1].sunset, set);
  snprintf_P(buf, sizeof(buf), PSTR("Aufg. %s  Unterg. %s"), rise, set);
  centered(d, 120, 228, 1, C_GRAY, buf);
}
