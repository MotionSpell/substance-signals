#pragma once

#include "lib_utils/tools.hpp"
#include <chrono>
#include <stdint.h>

namespace Modules {

struct IClock {
	static auto const Rate = 180000ULL;
	virtual uint64_t now() const = 0;
	virtual double getSpeed() const = 0;
};

class Clock : public IClock {
public:
	Clock(double speed) : timeStart(std::chrono::high_resolution_clock::now()), speed(speed) {}
	uint64_t now() const override;
	double getSpeed() const override;

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> const timeStart;
	double const speed;
};

extern IClock* const g_DefaultClock;

static uint64_t convertToTimescale(uint64_t time, uint64_t timescaleSrc, uint64_t timescaleDst) {
	return divUp<uint64_t>(time * timescaleDst, timescaleSrc);
}

static int64_t convertToTimescale(int64_t time, uint64_t timescaleSrc, uint64_t timescaleDst) {
	return divUp<int64_t>(time * timescaleDst, timescaleSrc);
}

static uint64_t timescaleToClock(uint64_t time, uint64_t timescale) {
	return convertToTimescale(time, timescale, IClock::Rate);
}

static int64_t timescaleToClock(int64_t time, uint64_t timescale) {
	return convertToTimescale(time, timescale, IClock::Rate);
}

static uint64_t clockToTimescale(uint64_t time, uint64_t timescale) {
	return convertToTimescale(time, IClock::Rate, timescale);
}

static int64_t clockToTimescale(int64_t time, uint64_t timescale) {
	return convertToTimescale(time, IClock::Rate, timescale);
}
}
