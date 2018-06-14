#pragma once

#include "lib_utils/fraction.hpp"
#include <functional>

typedef std::function<void(Fraction)> TaskFunc;

struct IScheduler {
	virtual ~IScheduler() {}
	virtual void scheduleAt(TaskFunc &&task, Fraction time) = 0;
	virtual void scheduleIn(TaskFunc &&task, Fraction time) = 0;
	//TODO: scheduleWhen() would allow to remove threads in modules, see https://github.com/gpac/signals/issues/14 + remark below on the necessity of thread pool + should be attached to any thread (e.g. th0).
};

void scheduleEvery(IScheduler* scheduler, TaskFunc &&task, Fraction loopTime, Fraction time);

