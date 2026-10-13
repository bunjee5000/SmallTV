#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "features/radar/RadarMode.h"
#include "features/radar/RadarClient.h"
#include "features/radar/RadarRoutes.h"
#include "Gfx.h"
#include <algorithm>
#include <iostream>

static Arduino_GFX display;
static Aircraft aircraft[MAX_AIRCRAFT];
static uint8_t count=1;
static uint32_t stamp=1;
static bool enrich=true;
static RadarRouteInfo info={"TK1554","HAJ","IST"};
Arduino_GFX* gfxDev() { return &display; }
uint16_t gfxTint(uint16_t c) { return c; }
int gfxTextW(const char* value,uint8_t size) { return strlen(value)*6*size; }
void gfxDrawCentered(const char* value,int y,uint8_t size,uint16_t color) {
  display.setTextSize(size); display.setTextColor(color);
  display.setCursor((240-gfxTextW(value,size))/2,y); display.print(value);
}
void gfxMessage(const char*,const char*,uint16_t) {}
void radarInit(const Settings&) {}
void radarForceRefresh() {}
void radarService(const Settings&) {}
uint8_t radarCount() { return count; }
const Aircraft& aircraftAt(uint8_t index) { return aircraft[index]; }
uint32_t radarLastOkMs() { return stamp; }
bool radarError() { return false; }
const RadarRouteInfo* radarRouteFor(const char*) { return enrich ? &info : nullptr; }
bool radarRoutesService(const Settings&) { return false; }

int main(int argc,char** argv) {
  display.clipGeometry=true;
  Settings s={}; s.radar.lat=52; s.radar.lon=10; s.radar.rangeKm=20;
  s.radar.showLabels=s.radar.showRoutes=s.radar.showVectors=s.radar.showRimDots=true;
  aircraft[0].distKm=7; aircraft[0].bearingDeg=270;
  aircraft[0].track=90; aircraft[0].gs=420; aircraft[0].altFt=35000;
  strcpy(aircraft[0].callsign,"THY1554"); s.radar.uiScale=1;
  g_radarMode.begin(s); g_radarMode.service(s);
  assert(std::find(display.strings.begin(),display.strings.end(),"TK 1554")!=display.strings.end());
  assert(std::find(display.strings.begin(),display.strings.end(),"HAJ-IST")!=display.strings.end());
  assert(std::find(display.strings.begin(),display.strings.end(),"FL350")!=display.strings.end());
  if(argc>1) display.save(argv[1]);
  unsigned cases=0;
  for(int scale=0;scale<3;++scale) for(int bearing=0;bearing<360;bearing+=5)
    for(int distance:{0,10,20,21}) for(bool routes:{false,true}) {
      s.radar.uiScale=scale; enrich=routes;
      aircraft[0].bearingDeg=bearing; aircraft[0].distKm=distance;
      ++stamp; g_radarMode.service(s); ++cases;
      if(distance<=20) {
        const char* flight=routes ? "TK 1554" : "THY1554";
        assert(std::find(display.strings.begin(),display.strings.end(),flight)!=display.strings.end());
        assert(std::find(display.strings.begin(),display.strings.end(),"FL350")!=display.strings.end());
      }
    }
  count=24;
  for(int i=0;i<24;++i) {
    aircraft[i]=aircraft[0]; aircraft[i].distKm=8+i*.3f; aircraft[i].bearingDeg=i*15;
    snprintf(aircraft[i].callsign,9,"THY%d",1000+i);
  }
  for(int scale=0;scale<3;++scale) { s.radar.uiScale=scale; ++stamp; g_radarMode.service(s); }
  enrich=false; count=1; strcpy(aircraft[0].callsign,"A"); aircraft[0].altFt=35000;
  ++stamp; g_radarMode.service(s);
  assert(std::find(display.strings.begin(),display.strings.end(),"A")!=display.strings.end());
  std::cout<<"Actual Radar renderer: "<<cases<<" edge/scale/route cases, dense traffic, flight/route/altitude text bounds: PASS\n";
}
