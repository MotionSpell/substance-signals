#pragma once

#include "lib_utils/system_clock.hpp"

struct RegulatorMultiConfig {
	int maxMediaTimeDelayInMs = 3000;
	int maxClockTimeDelayInMs = 3000;
	std::shared_ptr<IClock> clock = g_SystemClock;
};
