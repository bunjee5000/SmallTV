#pragma once
#include "Mode.h"
class ClockMode : public DisplayMode {
 public:
  const char* id() const override { return "clock"; }
  uint8_t modeConst() const override { return MODE_CLOCK; }
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override;
  void wake(const Settings&) override { lastMinute_ = -1; }
 private:
  int lastMinute_ = -1;
};
extern ClockMode g_clockMode;
