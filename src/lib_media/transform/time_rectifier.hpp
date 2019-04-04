#pragma once

#include <memory>
#include "lib_utils/fraction.hpp"

struct IClock;
struct IScheduler;

struct TimeRectifierConfig {
	std::shared_ptr<IClock> clock;
	IScheduler* scheduler;
	Fraction frameRate;
};

