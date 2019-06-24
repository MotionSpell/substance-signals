#pragma once

#include <cstdint>
#include <string>
#include "lib_utils/fraction.hpp"

#ifdef _WIN32
int gettimeofday(struct timeval *tp, void *tz);
#endif

Fraction getUTC();
uint64_t UTC2NTP(uint64_t absTimeUTCInMs);
void timeInMsToStr(uint64_t timestamp, char buffer[24], const char *msSeparator = ",");
std::string getDay();
std::string getTimeFromUTC();

struct IUtcClock {
	virtual Fraction getTime() = 0;
};

extern IUtcClock *g_UtcClock;
