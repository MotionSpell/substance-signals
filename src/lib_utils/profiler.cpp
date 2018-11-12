#include "profiler.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>

#define FORMAT(i, max) std::setw(1+(std::streamsize)log10(max)) << i

namespace Tools {
Profiler::Profiler(const std::string &name) : name(name) {
	startTime = std::chrono::high_resolution_clock::now();
}

Profiler::~Profiler() {
	std::cout << "[" << name.c_str() << "] " << FORMAT(elapsedInUs(), maxDurationInSec*unit) << " us" << std::endl;
}

uint64_t Profiler::elapsedInUs() {
	auto stopTime = std::chrono::high_resolution_clock::now();
	auto value = std::chrono::duration_cast<std::chrono::microseconds>(stopTime - startTime);
	return value.count();
}

}
