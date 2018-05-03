#include "clock.hpp"
#include <thread>

using namespace std::chrono;

Clock::Clock(double speed)
	: timeStart(high_resolution_clock::now()), speed(speed) {
}

Fraction Clock::now() const {
	auto const timeNow = high_resolution_clock::now();
	auto const timeElapsedInSpeed = speed * (timeNow - timeStart);
	auto const timeNowInMs = duration_cast<milliseconds>(timeElapsedInSpeed);
	return Fraction(timeNowInMs.count(), 1000);
}

double Clock::getSpeed() const {
	return speed;
}

void Clock::sleep(Fraction time) const {
	if (speed > 0.0) {
		std::this_thread::sleep_for(milliseconds(time.num * 1000 / time.den));
	}
}

extern const std::shared_ptr<IClock> g_DefaultClock(new Clock(1.0));
