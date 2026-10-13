#include "Arduino.h"
#include "features/radar/RadarRouteData.h"
#include "features/radar/RadarLabels.h"
#include <assert.h>
#include <iostream>

int main() {
  // Synthetic example matching the documented provider schema, not a route database.
  const char* text=R"({"response":{"flightroute":{"callsign":"THY1554","callsign_icao":"THY1554","callsign_iata":"TK1554","origin":{"iata_code":"HAJ","icao_code":"EDDV"},"destination":{"iata_code":"IST","icao_code":"LTFM"}}}})";
  JsonDocument doc; assert(!deserializeJson(doc,text));
  RadarRouteInfo info={}; assert(radarDecodeRoute(doc.as<JsonVariantConst>(),"THY1554",info));
  char label[10]; radarFlightLabel("THY1554",&info,label);
  assert(!strcmp(label,"TK 1554") && !strcmp(info.origin,"HAJ") && !strcmp(info.destination,"IST"));
  RadarRouteInfo old=info;
  assert(!radarDecodeRoute(doc.as<JsonVariantConst>(),"DLH400",info)); assert(!memcmp(&info,&old,sizeof(info)));
  JsonObject route=doc["response"]["flightroute"];
  route["callsign_iata"]=nullptr; route["origin"]["iata_code"]=nullptr;
  assert(radarDecodeRoute(doc.as<JsonVariantConst>(),"THY1554",info));
  radarFlightLabel("THY1554",&info,label); assert(!strcmp(label,"THY1554") && !strcmp(info.origin,"EDDV"));
  route["destination"]["iata_code"]="<x>"; route["destination"]["icao_code"]="bad!";
  old=info; assert(!radarDecodeRoute(doc.as<JsonVariantConst>(),"THY1554",info)); assert(!memcmp(&info,&old,sizeof(info)));
  for(const char* invalid:{"", "abc123", "THY/../", "123456", "THY123456789", "DABCD"}) assert(!radarRouteEligible(invalid));
  for(const char* good:{"THY1554","THY1WT","TK1554","U2123","6E123"}) assert(radarRouteEligible(good));
  RadarRouteCache cache;
  const char* active[]={"THY1554","DLH400"};
  uint32_t now=UINT32_MAX-1000;
  cache.remember(active[0],&info,now,2000,active,2);
  assert(cache.lookup(active[0],now+1000)); assert(!cache.due(active[0],now+1000));
  assert(!cache.lookup(active[0],now+2000)); assert(cache.due(active[0],now+2000));
  cache.remember(active[0],nullptr,now+2000,1000,active,2);
  assert(cache.lookup(active[0],now+2001)); // transient failure keeps same-flight data
  RadarRouteInfo empty={};
  cache.remember(active[1],&empty,now,3000,active,2);
  assert(!cache.lookup(active[1],now+100)); assert(!cache.due(active[1],now+100));
  assert(cache.due(active[1],now+3000));
  char names[MAX_AIRCRAFT][9]; const char* current[MAX_AIRCRAFT];
  for(int i=0;i<MAX_AIRCRAFT;i++) { snprintf(names[i],9,"TK%d",100+i); current[i]=names[i]; }
  for(int i=0;i<MAX_AIRCRAFT;i++) cache.remember(current[i],&info,100,1000,current,MAX_AIRCRAFT);
  for(int i=0;i<MAX_AIRCRAFT;i++) assert(cache.lookup(current[i],101));
  current[0]="TK999"; cache.remember(current[0],&info,100,1000,current,MAX_AIRCRAFT);
  for(int i=0;i<MAX_AIRCRAFT;i++) assert(cache.lookup(current[i],101));
  assert(!cache.lookup(names[0],101));
  // Real placement helper: all panel edges, line widths/heights and collisions.
  RadarLabelBox obstacles[26]; obstacles[0]={0,0,240,28}; obstacles[1]={115,115,10,10};
  for(int x:{8,120,232}) for(int y:{8,120,232}) for(int width:{6,30,54,108}) for(int height:{8,16,26,36}) {
    RadarLabelBox box;
    if(radarPlaceLabel(x,y,9,width,height,obstacles,2,-1,box)) {
      assert(box.x>=2 && box.x+box.w<=238 && box.y>=2 && box.y+box.h<=238);
      assert(!radarLabelHit(box,obstacles[0]) && !radarLabelHit(box,obstacles[1]));
      RadarLabelBox marker={(int16_t)(x-9),(int16_t)(y-9),18,18};
      assert(!radarLabelHit(box,marker));
    }
  }
  uint8_t n=2;
  for(int i=0;i<24;i++) { RadarLabelBox box;
    if(radarPlaceLabel(80+i*2,80+i,9,84,16,obstacles,n,-1,box)) obstacles[n++]=box; }
  for(int i=2;i<n;i++) { RadarLabelBox enlarged;
    if(radarPlaceLabel(100,100,9,84,36,obstacles,n,i,enlarged)) obstacles[i]=enlarged; }
  for(int i=2;i<n;i++) for(int j=0;j<i;j++) assert(!radarLabelHit(obstacles[i],obstacles[j]));
  std::cout<<"Route identity, airport/flight validation, cache expiry/eviction, negative caching, wraparound and label bounds: PASS\n";
}
