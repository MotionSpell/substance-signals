#include "clock.hpp"
#include <cassert>

namespace Modules {

using namespace std::chrono;

uint64_t Clock::now() const {
	auto const timeNow = high_resolution_clock::now();
	auto const timeElapsedInSpeed = speed * (timeNow - timeStart);
	auto const timeNowInMs = duration_cast<milliseconds>(timeElapsedInSpeed);
	assert(IClock::Rate % 1000 == 0);
	return (timeNowInMs.count() * IClock::Rate) / 1000LL;
}

double Clock::getSpeed() const {
	return speed;
}

static Clock systemClock(1.0);
extern IClock* const g_DefaultClock = &systemClock;
}
