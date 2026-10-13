#pragma once
#include <stdint.h>

// Platform-independent, allocation-free calculations. Minutes are relative to
// UTC midnight of the requested civil date and may lie outside 0..1439.
#define DASH_NO_TIME INT16_MIN
typedef struct {
  int16_t prayer[5]; // Fajr, Dhuhr, Asr, Maghrib, Isha
  int16_t sunrise, sunset;
  uint8_t adjusted;  // bit 0 = Fajr, bit 4 = Isha: night-portion adjustment
} DashboardDay;
typedef struct { int16_t year; uint8_t month, day; } DashboardHijri;
// cycle is normalized geocentric Moon-minus-Sun longitude (elongation), not
// elapsed lunar age. 0=new, .25=first quarter, .5=full, .75=last quarter.
// Illumination approximation: (1-cos(2*pi*cycle))/2. Primary phase labels
// span +/-3.6 degrees; intervening sectors describe crescents/gibbous phases.
typedef struct { uint8_t index; float cycle; } DashboardMoon;

#ifdef __cplusplus
extern "C" {
#endif
int32_t dashboardCivilDay(int year, int month, int day); // days since 1970-01-01
DashboardHijri dashboardHijri(int32_t civilDay, int correction);
DashboardMoon dashboardMoon(int64_t utcSeconds);
DashboardDay dashboardPrayerDay(int32_t civilDay, float lat, float lon,
                                float fajrAngle, float ishaAngle,
                                int asrFactor, int highLatitudeRule);
// rule: 0 = angle-based night portion, 1 = middle, 2 = seventh, 3 = none
int dashboardNextPrayer(const DashboardDay days[3], int32_t today,
                        int64_t now, int64_t* nextUtc, uint8_t* adjusted);
#ifdef __cplusplus
}
#endif
