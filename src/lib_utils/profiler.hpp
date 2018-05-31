#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace Tools {

class Profiler {
	public:
		Profiler(const std::string &name);
		~Profiler();

		uint64_t elapsedInUs();

	private:
		Profiler& operator= (const Profiler&) = delete;

		std::string name;
#ifdef _WIN32
		LARGE_INTEGER startTime;
#else
		struct timeval startTime;
#endif
		const int unit = 1000000;
		const int maxDurationInSec = 100;
};

}
