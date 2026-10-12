#pragma once
#include "Settings.h"
#include <time.h>
void dashboardInvalidate(const Settings& s);
void dashboardRender(const Settings& s, const struct tm& now);
