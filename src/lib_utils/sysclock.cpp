#include "clock.hpp"
#include "sysclock.hpp"
#include <thread>

using namespace std::chrono;

SystemClock::SystemClock(double speed)
	: timeStart(high_resolution_clock::now()), speed(speed) {
}

Fraction SystemClock::now() const {
	auto const timeNow = high_resolution_clock::now();
	auto const timeElapsedInSpeed = speed * (timeNow - timeStart);
	auto const timeNowInMs = duration_cast<milliseconds>(timeElapsedInSpeed);
	return Fraction(timeNowInMs.count(), 1000);
}

void SystemClock::sleep(Fraction time) const {
	if (speed > 0.0) {
		std::this_thread::sleep_for(milliseconds(time.num * 1000 / time.den));
	}
}

extern const std::shared_ptr<IClock> g_SystemClock(new SystemClock(1.0));
