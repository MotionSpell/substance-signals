#include "clock.hpp"
#include <thread>

using namespace std::chrono;

Clock::Clock(double speed)
: timeStart(std::chrono::high_resolution_clock::now()), speed(speed) {
}

uint64_t Clock::now(uint64_t timescale) const {
	auto const timeNow = high_resolution_clock::now();
	auto const timeElapsedInSpeed = speed * (timeNow - timeStart);
	auto const timeNowInMs = duration_cast<milliseconds>(timeElapsedInSpeed);
	static_assert(IClock::Rate % 1000 == 0, "IClock::Rate must be a multiple of 1000");
	return convertToTimescale(timeNowInMs.count(), 1000, timescale);
}

double Clock::getSpeed() const {
	return speed;
}

void Clock::sleep(uint64_t time, uint64_t timescale) const {
	if (speed > 0.0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(convertToTimescale(time, timescale, 1000)));
	}
}

static std::shared_ptr<Clock> systemClock(new Clock(1.0));
extern const std::shared_ptr<Clock> g_DefaultClock = systemClock;
