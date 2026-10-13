// Executes the actual Dashboard.cpp using a tiny headless graphics adapter.
#include "Dashboard.h"
#include "DashboardWeather.h"
#include "DashboardMath.h"
#include "Arduino_GFX_Library.h"
#include "Gfx.h"
#include <assert.h>
#include <algorithm>

static Arduino_GFX display;
static DashboardWeather weather;
time_t dashboardTestEpoch=1791815820;
int dashboardTestOffsetSeconds=7200;
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
  display.textMargin=4; display.checkTextOverlap=true;
  Settings settings;
  settings.dashboard={52.3759f,9.7320f,18,17,true,true,true,0,0,0,0};
  time_t stamp=time(nullptr);
  struct tm now;
  localtime_r(&stamp,&now);
  weather.temperature=12.5; weather.valid=true; weather.stage=WEATHER_OK; weather.code=1;
  dashboardRender(settings,now);
  assert(std::find(display.strings.begin(),display.strings.end(),"N\x84" "chstes Gebet")!=display.strings.end());
  assert(std::find(display.strings.begin(),display.strings.end(),"13 \xF8" "C")!=display.strings.end());
  for(const auto& value:display.strings) assert(value.find("AH")==std::string::npos);
  if(argc>1) display.save(argv[1]);
  dashboardTestEpoch+=86400;
  localtime_r(&dashboardTestEpoch,&now);
  dashboardInvalidate(settings); dashboardRender(settings,now);
  assert(dashboardMoon(dashboardTestEpoch).index==1);
  if(argc>1) display.save((std::string(argv[1])+".oct13.ppm").c_str());
  // Every calendar month, high-latitude rule, temperature width, weather icon,
  // and unavailable/stale state must fit the 240x240 screen using real text.
  const int codes[]={0,1,2,3,45,48,51,56,61,66,71,77,80,85,95,96,99,7};
  for(int month=0;month<12;month++) for(int rule=0;rule<4;rule++) for(int code:codes) {
    now.tm_mon=month; now.tm_mday=21;
    dashboardTestEpoch=(time_t)dashboardCivilDay(now.tm_year+1900,month+1,21)*86400+14*3600;
    settings.dashboard.highLatitude=rule;
    weather.code=code; weather.temperature=-99.9f; weather.observedUtc=1;
    dashboardInvalidate(settings); dashboardRender(settings,now);
    assert(display.textRows>=8);
  }
  settings.dashboard.lat=89; weather.valid=false;
  dashboardInvalidate(settings); dashboardRender(settings,now);
  settings.dashboard.lat=52.3759f; weather.valid=true;
  for(int weekday=0;weekday<7;++weekday) for(int method=0;method<5;++method)
    for(bool countdown:{false,true}) for(float temperature:{-100.0f,70.0f}) {
      now.tm_wday=weekday; now.tm_mon=2;
      dashboardTestEpoch=(time_t)dashboardCivilDay(now.tm_year+1900,3,now.tm_mday)*86400+14*3600;
      settings.dashboard.method=method; settings.dashboard.countdown=countdown;
      weather.temperature=temperature;
      dashboardInvalidate(settings); dashboardRender(settings,now);
    }
  for(bool enabled:{false,true}) for(bool valid:{false,true})
    for(int stage:{WEATHER_OK,WEATHER_CONNECT}) for(int observed:{0,1}) {
      settings.dashboard.weather=enabled; weather.valid=valid;
      weather.stage=(DashboardWeatherStage)stage; weather.observedUtc=observed;
      dashboardInvalidate(settings); dashboardRender(settings,now);
    }
  settings.dashboard.weather=settings.dashboard.countdown=true;
  settings.dashboard.method=0; settings.dashboard.highLatitude=0;
  weather.valid=true; weather.observedUtc=0; weather.stage=WEATHER_OK;
  unsigned prayers=0, phases=0;
  int illumination[8]={};
  const char* prayerNames[]={"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
  for(int sample=0;sample<40*12;sample++) {
    dashboardTestEpoch=1791815820+(time_t)sample*7200;
    localtime_r(&dashboardTestEpoch,&now);
    weather.temperature=sample%2 ? -12.5f : 23.5f;
    weather.code=codes[sample%18];
    dashboardInvalidate(settings); dashboardRender(settings,now);
    for(const auto& value:display.strings) {
      assert(value.find("Ber.")==std::string::npos && value.find("MWL")==std::string::npos);
      assert(value.find("AH")==std::string::npos);
      for(int p=0;p<5;++p) if(value.compare(0,strlen(prayerNames[p]),prayerNames[p])==0) {
        if(!(prayers&(1u<<p)) && argc>1)
          display.save((std::string(argv[1])+".prayer-"+prayerNames[p]+".ppm").c_str());
        prayers|=1u<<p;
      }
    }
    int phase=dashboardMoon(dashboardTestEpoch).index;
    if(!(phases&(1u<<phase))) {
      for(int y=96;y<=124;++y) for(int x=166;x<=194;++x)
        illumination[phase]+=display.pixels[y*240+x]==0xffff;
      if(argc>1) display.save((std::string(argv[1])+".moon-"+std::to_string(phase)+".ppm").c_str());
    }
    phases|=1u<<phase;
  }
  assert(prayers==31 && phases==255);
  assert(illumination[0]<illumination[2] && illumination[2]<illumination[4]);
  // Same UTC instant: moon pixels and phase labels must not depend on timezone.
  // Include date crossings, fractional-hour zones and Berlin's two DST offsets.
  for(int utcDay:{10,12,13,18,26}) for(int hour:{0,12,23}) {
    dashboardTestEpoch=(time_t)dashboardCivilDay(2026,10,utcDay)*86400+hour*3600;
    dashboardTestOffsetSeconds=0; localtime_r(&dashboardTestEpoch,&now);
    dashboardInvalidate(settings); dashboardRender(settings,now);
    auto original=display.pixels;
    for(int offset:{-12*3600,-7*3600,3600,7200,19800,14*3600}) {
      dashboardTestOffsetSeconds=offset; localtime_r(&dashboardTestEpoch,&now);
      dashboardInvalidate(settings); dashboardRender(settings,now);
      for(int y=96;y<170;++y) for(int x=128;x<232;++x)
        assert(original[y*240+x]==display.pixels[y*240+x]);
    }
  }
  dashboardTestOffsetSeconds=7200;
  // Polar winter has only a midday crossing: exercise a double-digit countdown.
  settings.dashboard.lat=89; settings.dashboard.highLatitude=3;
  dashboardTestEpoch=(time_t)dashboardCivilDay(2026,12,21)*86400+13*3600;
  localtime_r(&dashboardTestEpoch,&now);
  dashboardInvalidate(settings); dashboardRender(settings,now);
  bool longCountdown=false;
  for(const auto& value:display.strings) {
    unsigned hours;
    if(sscanf(value.c_str(),"in %u Std.",&hours)==1 && hours>=10) longCountdown=true;
  }
  assert(longCountdown);
  if(argc>1) display.save((std::string(argv[1])+".long-countdown.ppm").c_str());
  puts("German renderer: all five prayers/eight moon phases, long countdown, weather/calendar/polar cases: zero text clipping/overlap, 4 px safety margin, CP437 glyphs and phase illumination PASS.");
  return 0;
}
