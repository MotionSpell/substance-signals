#pragma once

#include <cstdint>

inline
int64_t pgcd(int64_t a, int64_t b) {
	return b ? pgcd(b, a%b) : a;
}

template<typename T>
static void simplifyFraction(T& num, T& den) {
	auto const gcd = pgcd(num, den);
	num /= gcd;
	den /= gcd;
}

struct Fraction {
	Fraction(int64_t num = 1, int64_t den = 1) : num(num), den(den) {
	}
	template<typename T>
	inline explicit operator T() const {
		return (T)num / (T)den;
	}
	inline Fraction operator+(const Fraction &frac) const {
		auto newNum = num * frac.den + frac.num * den;
		auto newDen = den * frac.den;
		simplifyFraction(newNum, newDen);
		return Fraction(newNum, newDen);
	}
	inline Fraction operator-(const Fraction &frac) const {
		auto newNum = num * frac.den - frac.num * den;
		auto newDen = den * frac.den;
		simplifyFraction(newNum, newDen);
		return Fraction(newNum, newDen);
	}
	inline Fraction operator*(const Fraction &frac) const {
		auto newNum = num * frac.num;
		auto newDen = den * frac.den;
		simplifyFraction(newNum, newDen);
		return Fraction(newNum, newDen);
	}
	inline Fraction operator/(const Fraction &frac) const {
		auto newNum = num * frac.den;
		auto newDen = den * frac.num;
		simplifyFraction(newNum, newDen);
		return Fraction(newNum, newDen);
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

