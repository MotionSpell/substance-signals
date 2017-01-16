#include "time.hpp"

#define GF_NTP_SEC_1900_TO_1970 2208988800ul

#ifdef _WIN32 
#include <sys/timeb.h>
#include <Winsock2.h>
int gettimeofday(struct timeval *tp, void *tz) {
	struct _timeb timebuffer;
	_ftime(&timebuffer);
	tp->tv_sec = (long)(timebuffer.time);
	tp->tv_usec = timebuffer.millitm * 1000;
	return 0;
}
#endif

static void getNTP(uint32_t *sec, uint32_t *frac) {
	uint64_t frac_part;
	struct timeval now;
	gettimeofday(&now, NULL);
	if (sec) {
		*sec = (uint32_t)(now.tv_sec) + GF_NTP_SEC_1900_TO_1970;
	}
	if (frac) {
		frac_part = now.tv_usec * 0xFFFFFFFFULL;
		frac_part /= 1000000;
		*frac = (uint32_t)(frac_part);
	}
}

uint64_t getUTCInMs() {
	uint64_t current_time;
	double msec;
	uint32_t sec, frac;
	getNTP(&sec, &frac);
	current_time = sec - GF_NTP_SEC_1900_TO_1970;
	current_time *= 1000;
	msec = (frac*1000.0) / 0xFFFFFFFF;
	current_time += (uint64_t)msec;
	return current_time;
}

uint64_t UTC2NTP(uint64_t absTimeUTCInMs) {
	const uint64_t sec = GF_NTP_SEC_1900_TO_1970 + absTimeUTCInMs / 1000;
	const uint64_t msec = absTimeUTCInMs - 1000 * (absTimeUTCInMs / 1000);
	const uint64_t frac = (msec * 0xFFFFFFFF) / 1000;
	return (sec << 32) + frac;
}
