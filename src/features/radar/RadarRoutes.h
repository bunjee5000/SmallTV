#pragma once
#include "Settings.h"
#include "RadarRouteData.h"

enum RadarRouteStage : uint8_t { ROUTE_IDLE, ROUTE_OK, ROUTE_UNKNOWN, ROUTE_LOW_HEAP, ROUTE_ERROR };
struct RadarRouteStatus {
  uint32_t attempts, minSampled, heapClosed, blockClosed;
  int16_t http;
  uint8_t stage;
};
const RadarRouteStatus& radarRouteStatus();
const RadarRouteInfo* radarRouteFor(const char* callsign);
bool radarRoutesService(const Settings& s); // returns true when labels may change
