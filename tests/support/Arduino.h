// Minimal host adapter used by the headless dashboard renderer only.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string>
#include <math.h>
using String = std::string;
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
#define FPSTR(s) (s)
#define memcpy_P memcpy
#define strcpy_P strcpy
#define strlen_P strlen
#define snprintf_P snprintf
#define PI 3.14159265358979323846
inline void yield() {}
// Fixed 12 October 2026 16:37 CEST for a reproducible visual fixture.
extern time_t dashboardTestEpoch;
inline time_t dashboardTestTime(time_t*) { return dashboardTestEpoch; }
inline struct tm* dashboardTestLocaltime(const time_t* stamp, struct tm* out) {
  time_t local = *stamp + 7200;
  *out = *gmtime(&local);
  return out;
}
#define time dashboardTestTime
#define localtime_r dashboardTestLocaltime
