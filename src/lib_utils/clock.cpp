#include "clock.hpp"
#include <cassert>

using namespace std::chrono;

uint64_t Clock::now(const uint64_t timescale) const {
	auto const timeNow = high_resolution_clock::now();
	auto const timeElapsedInSpeed = speed * (timeNow - timeStart);
	auto const timeNowInMs = duration_cast<milliseconds>(timeElapsedInSpeed);
	static_assert(IClock::Rate % 1000 == 0, "IClock::Rate must be a multiple of 1000");
	return convertToTimescale(timeNowInMs.count(), 1000, timescale);
}

double Clock::getSpeed() const {
	return speed;
}

static std::shared_ptr<Clock> systemClock(new Clock(1.0));
extern const std::shared_ptr<Clock> g_DefaultClock = systemClock;
