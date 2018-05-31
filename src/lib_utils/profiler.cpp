#include "profiler.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>

#define FORMAT(i, max) std::setw(1+(std::streamsize)log10(max)) << i

namespace Tools {
Profiler::Profiler(const std::string &name) : name(name) {
#ifdef _WIN32
	QueryPerformanceCounter(&startTime);
#else
	gettimeofday(&startTime, nullptr);
#endif
}

Profiler::~Profiler() {
	std::cout << "[" << name.c_str() << "] " << FORMAT(elapsedInUs(), maxDurationInSec*unit) << " us" << std::endl;
}

uint64_t Profiler::elapsedInUs() {
#ifdef _WIN32
	LARGE_INTEGER stopTime;
	QueryPerformanceCounter(&stopTime);
	LARGE_INTEGER countsPerSecond;
	QueryPerformanceFrequency(&countsPerSecond);
	return (uint64_t)((unit * (stopTime.QuadPart - startTime.QuadPart)) / countsPerSecond.QuadPart);
#else
	struct timeval stopTime;
	gettimeofday(&stopTime, nullptr);
	return ((uint64_t)stopTime.tv_sec * 1000000 + stopTime.tv_usec) - ((uint64_t)startTime.tv_sec * 1000000 + startTime.tv_usec);
#endif
}

}
