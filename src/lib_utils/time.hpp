#pragma once

#include <cinttypes>
#include <string>

#ifdef _WIN32
int gettimeofday(struct timeval *tp, void *tz);
#endif

uint64_t getUTCInMs();
uint64_t UTC2NTP(uint64_t absTimeUTCInMs);
void timeInMsToStr(uint64_t timestamp, char buffer[24], const char *msSeparator = ",");
std::string getDay();
std::string getTimeFromUTC();
