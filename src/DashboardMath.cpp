#include "DashboardMath.h"
#include <math.h>

// Solar coordinates: USNO's approximate solar algorithm. Prayer definitions:
// https://praytimes.org/calculation . No timetable-specific offsets are implied.
static const float RAD = 0.017453292519943295f;
static float norm(float v, float period) { return v - period * floorf(v / period); }
static float sind(float v) { return sinf(v * RAD); }
static float cosd(float v) { return cosf(v * RAD); }
static float absolute(float v) { return v < 0 ? -v : v; }

int32_t dashboardCivilDay(int year, int month, int day) {
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400
         - 32045 - 2440588;
}

// Tabular civil Islamic calendar, epoch JDN 1948440 (16 July 622 Julian).
// Independent of moon illumination and local sighting/official announcements.
static int32_t hijriStart(int year, int month) {
  return 1948440 + 354 * (year - 1) + (3 + 11 * year) / 30
         + (59 * (month - 1) + 1) / 2;
}
DashboardHijri dashboardHijri(int32_t civilDay, int correction) {
  int32_t jdn = civilDay + 2440588 + correction;
  int year = (30 * (jdn - 1948440) + 10646) / 10631;
  while (jdn < hijriStart(year, 1)) --year;
  while (jdn >= hijriStart(year + 1, 1)) ++year;
  int month = 1;
  while (month < 12 && jdn >= hijriStart(year, month + 1)) ++month;
  DashboardHijri result = {(int16_t)year, (uint8_t)month,
                           (uint8_t)(jdn - hijriStart(year, month) + 1)};
  return result;
}

static float solarLongitude(float d) {
  float g = norm(357.529f + 0.98560028f * d, 360);
  return norm(280.459f + 0.98564736f * d + 1.915f * sind(g) + 0.020f * sind(2*g), 360);
}
static void solar(float d, float* declination, float* equation) {
  float q = norm(280.459f + 0.98564736f * d, 360);
  float longitude = solarLongitude(d);
  float e = 23.439f - 0.00000036f * d;
  float ra = norm(atan2f(cosd(e) * sind(longitude), cosd(longitude)) / RAD, 360);
  *declination = asinf(sind(e) * sind(longitude)) / RAD;
  *equation = norm(q - ra + 180, 360) / 15 - 12;
}

DashboardMoon dashboardMoon(int64_t utcSeconds) {
  // Truncated lunar longitude series (principal perturbations), not a fixed
  // synodic-month counter. Suitable for eight broad phase sectors, not timings
  // of conjunctions, eclipse prediction, or a religious calendar.
  // Subtract the epoch before conversion, retaining precision on this MCU.
  float d = (float)(utcSeconds / 86400 - 10957) - 0.5f
            + (float)(utcSeconds % 86400) / 86400.0f; // J2000 noon
  float l = norm(218.3164477f + 13.17639648f*d, 360);
  float m = norm(134.9633964f + 13.06499295f*d, 360);
  float s = norm(357.5291092f + 0.98560028f*d, 360);
  float e = norm(297.8501921f + 12.19074912f*d, 360);
  float f = norm(93.2720950f + 13.22935024f*d, 360);
  l += 6.289f*sind(m) + 1.274f*sind(2*e-m) + 0.658f*sind(2*e)
       + 0.214f*sind(2*m) - 0.186f*sind(s) - 0.114f*sind(2*f)
       + 0.059f*sind(2*e-2*m) + 0.057f*sind(2*e-s-m)
       + 0.053f*sind(2*e+m) + 0.046f*sind(2*e-s)
       + 0.041f*sind(s-m) - 0.035f*sind(e) - 0.031f*sind(s+m);
  float cycle = norm(l - solarLongitude(d), 360) / 360;
  DashboardMoon result = {(uint8_t)((int)floorf(cycle * 8 + 0.5f) % 8), (float)cycle};
  return result;
}

// UTC hours, refined with declination/equation at the event itself.
static float eventHour(int32_t day, float lat, float lon, float altitude,
                        int direction, int asrFactor) {
  float hour = 12 - lon / 15;
  for (int i = 0; i < 3; ++i) {
    float dec, eq;
    solar(day - 10957.5f + hour / 24, &dec, &eq);
    float noon = 12 - lon / 15 - eq;
    if (direction == 0) { hour = noon; continue; }
    float a = altitude;
    if (asrFactor) {
      float noonZenith = absolute(lat - dec);
      // The shadow formula requires the sun above the horizon at transit.
      // Beyond 90 degrees, tan wraps negative and invents a nighttime Asr.
      if (noonZenith >= 90) return NAN;
      a = atanf(1 / (asrFactor + tanf(noonZenith * RAD))) / RAD;
    }
    float denominator = cosd(lat) * cosd(dec);
    if (absolute(denominator) < 1e-10f) return NAN;
    float c = (sind(a) - sind(lat) * sind(dec)) / denominator;
    if (c < -1 || c > 1) return NAN; // genuine absent crossing, never clamp
    hour = noon + direction * acosf(c) / RAD / 15;
  }
  return hour;
}
static int16_t minute(float hour) {
  return isfinite(hour) ? (int16_t)lroundf(hour * 60) : DASH_NO_TIME;
}
static float portion(int rule, float angle) {
  if (rule == 1) return 0.5f;
  if (rule == 2) return 1.0f / 7;
  return angle / 60;
}
DashboardDay dashboardPrayerDay(int32_t day, float lat, float lon,
                                float fajrAngle, float ishaAngle,
                                int asrFactor, int rule) {
  DashboardDay result;
  result.adjusted = 0;
  float rise = eventHour(day, lat, lon, -0.833f, -1, 0);
  float set = eventHour(day, lat, lon, -0.833f, 1, 0);
  float fajr = eventHour(day, lat, lon, -fajrAngle, -1, 0);
  float isha = eventHour(day, lat, lon, -ishaAngle, 1, 0);
  if (rule != 3 && isfinite(rise) && isfinite(set)) {
    float previousSet = eventHour(day-1, lat, lon, -0.833f, 1, 0) - 24;
    float nextRise = eventHour(day+1, lat, lon, -0.833f, -1, 0) + 24;
    if (isfinite(previousSet)) {
      float limit = rise - portion(rule, fajrAngle) * (rise - previousSet);
      if (!isfinite(fajr) || fajr < limit) { fajr = limit; result.adjusted |= 1; }
    }
    if (isfinite(nextRise)) {
      float limit = set + portion(rule, ishaAngle) * (nextRise - set);
      if (!isfinite(isha) || isha > limit) { isha = limit; result.adjusted |= 16; }
    }
  }
  result.prayer[0] = minute(fajr);
  result.prayer[1] = minute(eventHour(day, lat, lon, 0, 0, 0));
  result.prayer[2] = minute(eventHour(day, lat, lon, 0, 1, asrFactor));
  result.prayer[3] = minute(set);
  result.prayer[4] = minute(isha);
  result.sunrise = minute(rise);
  result.sunset = minute(set);
  return result;
}

int dashboardNextPrayer(const DashboardDay days[3], int32_t today,
                        int64_t now, int64_t* nextUtc, uint8_t* adjusted) {
  int found = -1;
  *nextUtc = INT64_MAX;
  *adjusted = 0;
  // Include yesterday's Isha: at some longitudes/summer latitudes it occurs
  // after local midnight. Absolute epochs also make DST countdowns correct.
  for (int d = 0; d < 3; ++d) {
    for (int p = 0; p < 5; ++p) {
      if (days[d].prayer[p] == DASH_NO_TIME) continue;
      int64_t epoch = (int64_t)(today + d - 1) * 86400 + days[d].prayer[p] * 60;
      if (epoch >= now && epoch < *nextUtc) {
        *nextUtc = epoch;
        found = p;
        *adjusted = (days[d].adjusted >> p) & 1;
      }
    }
  }
  return found;
}
