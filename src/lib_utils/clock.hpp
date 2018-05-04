#pragma once

#include "lib_utils/tools.hpp"

struct IClock {
	static auto const Rate = 180000ULL;
	virtual Fraction now() const = 0;
	virtual void sleep(Fraction time) const = 0;

	// FIXME: what are the units of this?
	// how is the caller supposed to do anything with this value,
	// without needing another time reference? (i.e another IClock)
	virtual double getSpeed() const = 0;
};

extern const std::shared_ptr<IClock> g_DefaultClock;

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

static inline int64_t fractionToClock(Fraction time) {
	return timescaleToClock(time.num, time.den);
}
