#include "Dashboard.h"
#include "DashboardMath.h"
#include "DashboardWeather.h"
#include "Gfx.h"
#include <Arduino_GFX_Library.h>
#include <math.h>

static DashboardDay g_days[3];
static int32_t g_day = INT32_MIN;
static const char hijriMonths[][20] PROGMEM = {
  "Muharram", "Safar", "Rabi al-Awwal", "Rabi al-Thani", "Jumada al-Awwal",
  "Jumada al-Thani", "Rajab", "Sha'ban", "Ramadan", "Shawwal", "Dhu al-Qadah", "Dhu al-Hijjah"
};
static const char moonNames[][18] PROGMEM = {
  "New moon", "Waxing crescent", "First quarter", "Waxing gibbous",
  "Full moon", "Waning gibbous", "Last quarter", "Waning crescent"
};
static const char prayerNames[][8] PROGMEM = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
static const char methodNames[][8] PROGMEM = {"MWL", "ISNA", "Egypt", "Karachi", "Custom"};
static const char weekdays[][4] PROGMEM = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char months[][4] PROGMEM = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void dashboardInvalidate(const Settings& s) {
  g_day = INT32_MIN;
  dashboardWeatherApply(s.dashboard);
}
static void text(Arduino_GFX* d, int x, int y, uint8_t size, uint16_t color, const char* value) {
  d->setTextSize(size); d->setTextColor(color); d->setCursor(x, y); d->print(value);
}
static void flashText(Arduino_GFX* d, int x, int y, uint8_t size, uint16_t color, const char* value) {
  d->setTextSize(size); d->setTextColor(color); d->setCursor(x, y); d->print(FPSTR(value));
}
static void centerFlash(Arduino_GFX* d, int y, uint8_t size, uint16_t color, const char* value) {
  flashText(d, (240 - strlen_P(value) * 6 * size) / 2, y, size, color, value);
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
  "Clear", "Partly cloudy", "Cloudy", "Rain", "Snow", "Fog", "Thunderstorm", "Unknown"
};
static void weatherIcon(Arduino_GFX* d, int kind) {
  const int x = 23, y = 123;
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
  const int cx = 23, cy = 162, radius = 11;
  d->fillCircle(cx, cy, radius, C_DGRAY);
  float terminator = cosf(cycle * 2 * (float)PI);
  for (int y = -radius+1; y < radius; ++y) {
    int half = (int)sqrtf((float)(radius*radius - y*y));
    int edge = lroundf(terminator * half);
    int left = cycle < 0.5f ? edge : -half;
    int right = cycle < 0.5f ? half : -edge;
    if (right > left) d->drawFastHLine(cx+left, cy+y, right-left, C_WHITE);
  }
  d->drawCircle(cx, cy, radius, C_GRAY);
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
  char buf[64], name[20];
  d->setTextWrap(false);
  d->fillScreen(C_BLACK);
  snprintf_P(buf, sizeof(buf), PSTR("%02d:%02d"), now.tm_hour, now.tm_min);
  gfxDrawCentered(buf, 12, 5, C_WHITE);
  // Avoid strftime: linking its locale machinery costs static RAM on ESP8266.
  char weekday[4], month[4];
  memcpy_P(weekday, weekdays[now.tm_wday], sizeof(weekday));
  memcpy_P(month, months[now.tm_mon], sizeof(month));
  snprintf_P(buf, sizeof(buf), PSTR("%s %02d %s %d"), weekday, now.tm_mday, month, now.tm_year+1900);
  gfxDrawCentered(buf, 60, 2, C_GRAY);
  DashboardHijri hijri = dashboardHijri(day, s.hijriCorrection);
  memcpy_P(name, hijriMonths[hijri.month-1], sizeof(name));
  snprintf_P(buf, sizeof(buf), PSTR("%u %s %d AH~"), hijri.day, name, hijri.year);
  gfxDrawCentered(buf, 84, 1, accent);
  d->drawFastHLine(12, 100, 216, C_DGRAY);

  const DashboardWeather& weather = dashboardWeather();
  if (s.weather && weather.valid) {
    int kind = weatherKind(weather.code);
    weatherIcon(d, kind);
    snprintf_P(buf, sizeof(buf), PSTR("%.1f C"), weather.temperature);
    text(d, 48, 109, 2, C_WHITE, buf);
    flashText(d, 48, 133, 1, C_GRAY, weatherNames[kind]);
    if (dashboardWeatherStale()) flashText(d, 178, 133, 1, accent, PSTR("stale"));
    else if (weather.stage != WEATHER_OK) flashText(d, 178, 133, 1, C_GRAY, PSTR("retry"));
  } else {
    weatherIcon(d, 7);
    flashText(d, 48, 111, 2, C_WHITE, PSTR("--.- C"));
    flashText(d, 48, 133, 1, C_GRAY, s.weather ? PSTR("Weather pending") : PSTR("Weather off"));
  }
  DashboardMoon moon = dashboardMoon(epoch);
  moonIcon(d, moon.cycle);
  flashText(d, 48, 155, 2, C_WHITE, moonNames[moon.index]);
  d->drawFastHLine(12, 182, 216, C_DGRAY);

  int64_t next;
  uint8_t adjusted;
  int prayer = dashboardNextPrayer(g_days, day, epoch, &next, &adjusted);
  char hhmm[6];
  if (prayer >= 0) {
    memcpy_P(name, prayerNames[prayer], sizeof(prayerNames[0]));
    clockText(next, hhmm);
    snprintf_P(buf, sizeof(buf), PSTR("Next %s %s"), name, hhmm);
    gfxDrawCentered(buf, 189, 2, C_WHITE);
    memcpy_P(name, methodNames[s.method], sizeof(methodNames[0]));
    if (s.countdown) {
      unsigned minutes = (unsigned)((next - epoch + 59) / 60);
      snprintf_P(buf, sizeof(buf), PSTR("Calc %s%c | in %02uh %02um"), name, adjusted ? '*' : ' ',
                 minutes/60, minutes%60);
    } else snprintf_P(buf, sizeof(buf), PSTR("Calculated %s%c"), name, adjusted ? '*' : ' ');
    gfxDrawCentered(buf, 211, 1, accent);
  } else {
    centerFlash(d, 189, 2, C_WHITE, PSTR("No prayer crossing"));
    centerFlash(d, 211, 1, accent, PSTR("Check latitude / convention"));
  }
  char rise[6], set[6];
  sunText(day, g_days[1].sunrise, rise); sunText(day, g_days[1].sunset, set);
  snprintf_P(buf, sizeof(buf), PSTR("Rise %s   Set %s"), rise, set);
  gfxDrawCentered(buf, 230, 1, C_GRAY);
}
