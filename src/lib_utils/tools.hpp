#pragma once

#include <string>
#include <memory>
#include <typeinfo>
#include <vector>

inline
std::string string2hex(const uint8_t *extradata, size_t extradataSize) {
	static const char* const ab = "0123456789ABCDEF";
	std::string output;
	output.reserve(2 * extradataSize);
	for (size_t i = 0; i < extradataSize; ++i) {
		auto const c = extradata[i];
		output.push_back(ab[c >> 4]);
		output.push_back(ab[c & 15]);
	}
	return output;
}

template<typename T>
std::vector<T> makeVector(std::initializer_list<T> list) {
	return std::vector<T>(list);
}

using std::make_unique;
using std::make_shared;

template<class T>
std::unique_ptr<T> uptr(T *p) {
	return std::unique_ptr<T>(p);
}

template<class T, class U>
std::shared_ptr<T> safe_cast(std::shared_ptr<U> p) {
	if (!p)
		return nullptr;
	auto r = std::dynamic_pointer_cast<T>(p);
	if (!r) {
		auto msg = std::string("dynamic cast error: could not convert from '") + typeid(U).name() + "' to '" + typeid(T).name() + "'";
		throw std::runtime_error(msg);
	}
	return r;
}

template<typename T, typename U>
T* safe_cast(U *p) {
	if (!p)
		return nullptr;
	auto r = dynamic_cast<T*>(p);
	if (!r) {
		auto msg = std::string("dynamic cast error: could not convert from '") + typeid(U).name() + "' to '" + typeid(T).name() + "'";
		throw std::runtime_error(msg);
	}
	return r;
}

template<typename T>
constexpr T operator | (T a, T b) {
	return static_cast<T>(static_cast<int>(a) | static_cast<int>(b));
}

template<typename T>
constexpr T operator & (T a, T b) {
	return static_cast<T>(static_cast<int>(a) & static_cast<int>(b));
}

inline
void enforce(bool condition, const char* msg) {
	if (!condition)
		throw std::runtime_error(msg);
}
