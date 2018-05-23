#if _WIN32

#include <windows.h>
bool setHighThreadPriority() {
	return SetThreadPriority(NULL, THREAD_PRIORITY_TIME_CRITICAL);
}
#else

#include <pthread.h>
bool setHighThreadPriority() {
	sched_param sp {};
	sp.sched_priority = 1;
	if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp))
		return false;

	return true;
}
#endif
