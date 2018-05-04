#pragma once

#include <cstdint>

inline
int64_t pgcd(int64_t a, int64_t b) {
	return b ? pgcd(b, a%b) : a;
}

struct Fraction {
	Fraction(int64_t num = 1, int64_t den = 1) : num(num), den(den) {
	}
	template<typename T>
	inline explicit operator T() const {
		return (T)num / (T)den;
	}
	inline Fraction operator+(const Fraction &frac) const {
		auto const gcd = pgcd(num * frac.den + frac.num * den, den * frac.den);
		return Fraction((num * frac.den + frac.num * den) / gcd, (den * frac.den) / gcd);
	}
	inline Fraction operator-(const Fraction &frac) const {
		auto const gcd = pgcd(num * frac.den - frac.num * den, den * frac.den);
		return Fraction((num * frac.den - frac.num * den) / gcd, (den * frac.den) / gcd);
	}
	inline Fraction operator*(const Fraction &frac) const {
		auto const gcd = pgcd(num * frac.num, den * frac.den);
		return Fraction((num * frac.num) / gcd, (den * frac.den) / gcd);
	}
	inline Fraction operator/(const Fraction &frac) const {
		auto const gcd = pgcd(num * frac.den, den * frac.num);
		return Fraction((num * frac.den) / gcd, (den * frac.num) / gcd);
	}
	inline bool operator==(const Fraction& rhs) const {
		return num * rhs.den == rhs.num * den;
	}
	inline bool operator!=(const Fraction& rhs) const {
		return !(rhs == *this);
	}
	template <typename T>
	inline bool operator==(const T& rhs) const {
		return *this == Fraction(rhs);
	}
	inline bool operator<(const Fraction& rhs) const  {
		return num * rhs.den < rhs.num * den;
	}
	inline bool operator>(const Fraction& rhs) const {
		return num * rhs.den > rhs.num * den;
	}
	inline bool operator<=(const Fraction& rhs) const {
		return !(*this > rhs);
	}
	inline bool operator>=(const Fraction& rhs) const {
		return !(*this < rhs);
	}
	Fraction inverse() const {
		return Fraction(den, num);
	}

	int64_t num;
	int64_t den;
};

template <typename T>
constexpr
int sign(T num) {
	return (T(0) < num) - (num < T(0));
}

template<typename T>
T divUp(T num, T divisor) {
	return (num + sign(num) * (divisor - 1)) / divisor;
}

