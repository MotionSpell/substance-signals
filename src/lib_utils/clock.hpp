#pragma once

#include "lib_utils/fraction.hpp"

struct IClock {
	virtual ~IClock() = default;
	static auto const Rate = 180000ULL;
	virtual Fraction now() const = 0;
	virtual void sleep(Fraction time) const = 0;
};

template<typename T>
static T convertToTimescale(T time, uint64_t timescaleSrc, uint64_t timescaleDst) {
	simplifyFraction(timescaleSrc, timescaleDst);
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

static inline int64_t fractionToTimescale(Fraction time, uint64_t timescale) {
	return convertToTimescale(time.num, time.den, timescale);
}

static inline int64_t fractionToClock(Fraction time) {
	return timescaleToClock(time.num, time.den);
}

