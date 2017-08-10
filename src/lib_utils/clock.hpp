#pragma once

#include "lib_utils/tools.hpp"
#include <chrono>

struct IClock {
	static auto const Rate = 180000ULL;
	virtual uint64_t now(uint64_t timescale = IClock::Rate) const = 0;
	virtual double getSpeed() const = 0;
	virtual void sleep(uint64_t time, uint64_t timescale = IClock::Rate) const = 0;
};

class Clock : public IClock {
public:
	Clock(double speed);
	uint64_t now(uint64_t timescale = 1000) const override;
	double getSpeed() const override;
	void sleep(uint64_t timeIn180k, uint64_t timescale = IClock::Rate) const override;

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> const timeStart;
	double const speed;
};

extern const std::shared_ptr<Clock> g_DefaultClock;

template<typename T>
static T convertToTimescale(T time, uint64_t timescaleSrc, uint64_t timescaleDst) {
	return divUp<T>(time * timescaleDst, timescaleSrc);
}

template<typename T>
static T timescaleToClock(T time, uint64_t timescale) {
	return convertToTimescale(time, timescale, IClock::Rate);
}

template<typename T>
static T clockToTimescale(T time, uint64_t timescale) {
	return convertToTimescale(time, IClock::Rate, timescale);
}
