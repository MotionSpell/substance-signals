#pragma once

#include <memory>
#include "lib_utils/fraction.hpp"

struct IClock;
struct IScheduler;

struct RectifierConfig {
	std::shared_ptr<IClock> clock;
	IScheduler* scheduler;
	Fraction frameRate;
};

