#pragma once

#include <cinttypes>
#include <string>
#include "tools.hpp"

#ifdef _WIN32
int gettimeofday(struct timeval *tp, void *tz);
#endif

Fraction getUTC();
uint64_t UTC2NTP(uint64_t absTimeUTCInMs);
void timeInMsToStr(uint64_t timestamp, char buffer[24], const char *msSeparator = ",");
std::string getDay();
std::string getTimeFromUTC();
