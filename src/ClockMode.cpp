// Lightweight digital clock, deliberately no heap-allocating theme engine.
#include "ClockMode.h"
#include "Clock.h"
#include "Gfx.h"
#include "config.h"
#include <Arduino_GFX_Library.h>

ClockMode g_clockMode;

void ClockMode::service(const Settings&) {
  Arduino_GFX* d = gfxDev();
  if (!d) return;
  struct tm now;
  if (!clockNow(now)) {
    if (lastMinute_ != -2) {
      d->fillScreen(C_BLACK);
      gfxDrawCentered("HANNOVER", 42, 2, C_BLUE);
      gfxDrawCentered("Syncing time...", 116, 2, C_WHITE);
      lastMinute_ = -2;
    }
    return;
  }
  // Minute-granularity repaints keep the display stable and reduce work.
  int minuteKey = now.tm_yday * 1440 + now.tm_hour * 60 + now.tm_min;
  if (lastMinute_ == minuteKey) return;
  lastMinute_ = minuteKey;
  d->fillScreen(C_BLACK);
  gfxDrawCentered("HANNOVER", 36, 2, C_BLUE);
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
  gfxDrawCentered(buf, 91, 5, C_WHITE);
  static const char days[][4] PROGMEM = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char months[][4] PROGMEM = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char day[4], month[4];
  memcpy_P(day, days[now.tm_wday], sizeof(day));
  memcpy_P(month, months[now.tm_mon], sizeof(month));
  snprintf(buf, sizeof(buf), "%s %02d %s %d", day, now.tm_mday, month, now.tm_year + 1900);
  gfxDrawCentered(buf, 167, 2, C_GRAY);
  gfxDrawCentered("RADAR + CLOCK", 212, 1, C_BLUE);
}
