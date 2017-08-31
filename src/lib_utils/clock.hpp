#pragma once

#include "lib_utils/tools.hpp"
#include <chrono>

struct IClock {
	static auto const Rate = 180000ULL;
	virtual Fraction now() const = 0;
	virtual double getSpeed() const = 0;
	virtual void sleep(Fraction time) const = 0;
};

class Clock : public IClock {
public:
	Clock(double speed);
	Fraction now() const override;
	double getSpeed() const override;
	void sleep(Fraction time) const override;

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> const timeStart;
	double const speed;
};

extern const std::shared_ptr<Clock> g_DefaultClock;

template<typename T>
static T convertToTimescale(T time, uint64_t timescaleSrc, uint64_t timescaleDst) {
	auto const gcd = pgcd(timescaleSrc, timescaleDst);
	return divUp<T>(time * (timescaleDst / gcd), (timescaleSrc / gcd));
}

template<typename T>
static T timescaleToClock(T time, uint64_t timescale) {
	return convertToTimescale(time, timescale, IClock::Rate);
}

template<typename T>
static T clockToTimescale(T time, uint64_t timescale) {
	return convertToTimescale(time, IClock::Rate, timescale);
}

static int64_t fractionToClock(Fraction time) {
	return timescaleToClock(time.num, time.den);
}
