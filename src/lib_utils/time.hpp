#pragma once

#include <cinttypes>

#ifdef _WIN32
int gettimeofday(struct timeval *tp, void *tz);
#endif

uint64_t getUTCInMs();
uint64_t UTC2NTP(uint64_t absTimeUTCInMs);
