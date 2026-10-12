// Executes the actual Dashboard.cpp using a tiny headless graphics adapter.
#include "Dashboard.h"
#include "DashboardWeather.h"
#include "Arduino_GFX_Library.h"
#include "Gfx.h"
#include <assert.h>

static Arduino_GFX display;
static DashboardWeather weather;
time_t dashboardTestEpoch=1791815820;
Arduino_GFX* gfxDev() { return &display; }
uint16_t gfxTint(uint16_t value) { return value; }
int gfxTextW(const char* value, uint8_t size) { return strlen(value)*6*size; }
void gfxDrawCentered(const char* value,int y,uint8_t size,uint16_t color) {
  display.setCursor((240-gfxTextW(value,size))/2,y);
  display.setTextSize(size); display.setTextColor(color); display.print(value);
}
const DashboardWeather& dashboardWeather() { return weather; }
bool dashboardWeatherStale() { return weather.observedUtc==1; }
void dashboardWeatherApply(const DashboardSettings&) {}
int main(int argc,char** argv) {
  Settings settings;
  settings.dashboard={52.3759f,9.7320f,18,17,true,true,true,0,0,0,0};
  time_t stamp=time(nullptr);
  struct tm now;
  localtime_r(&stamp,&now);
  weather.temperature=12.5; weather.valid=true; weather.stage=WEATHER_OK; weather.code=1;
  dashboardRender(settings,now);
  if(argc>1) display.save(argv[1]);
  // Every calendar month, high-latitude rule, temperature width, weather icon,
  // and unavailable/stale state must fit the 240x240 screen using real text.
  const int codes[]={0,1,2,3,45,48,51,56,61,66,71,77,80,85,95,96,99,7};
  for(int month=0;month<12;month++) for(int rule=0;rule<4;rule++) for(int code:codes) {
    now.tm_mon=month; now.tm_mday=21;
    settings.dashboard.highLatitude=rule;
    weather.code=code; weather.temperature=-99.9f; weather.observedUtc=1;
    dashboardInvalidate(settings); dashboardRender(settings,now);
    assert(display.textRows>=8);
  }
  settings.dashboard.lat=89; weather.valid=false;
  dashboardInvalidate(settings); dashboardRender(settings,now);
  settings.dashboard.lat=52.3759f; weather.valid=true;
  for(int day=0;day<40;day++) {
    dashboardTestEpoch=1791815820+(time_t)day*86400;
    localtime_r(&dashboardTestEpoch,&now);
    dashboardInvalidate(settings); dashboardRender(settings,now);
  }
  puts("Actual dashboard renderer: 864 icon/calendar/rule cases plus pending/polar state fit 240x240 ASCII bounds.");
  return 0;
}
