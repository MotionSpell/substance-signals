#include "scheduler.hpp"
#include "lib_utils/format.hpp"

auto const NEVER = Fraction(-1, 1);

Scheduler::Scheduler(std::shared_ptr<IClock> clock, std::shared_ptr<ITimer> timer) : timer(timer), clock(clock) {
	nextWakeUpTime = NEVER;
}

void Scheduler::scheduleAt(TaskFunc &&task, Fraction time) {
	{
		std::unique_lock<std::mutex> lock(mutex);
		queue.push(Task(std::move(task), time));
	}
	reschedule();
}

namespace {
void runAndReschedule(IScheduler* scheduler, TaskFunc task, Fraction loopTime, Fraction timeNow) {
	task(timeNow);
	scheduleEvery(scheduler, std::move(task), loopTime, timeNow + loopTime);
}
}

void scheduleEvery(IScheduler* scheduler, TaskFunc &&task, Fraction loopTime, Fraction time) {
	auto schedTask = std::bind(&runAndReschedule, scheduler, std::move(task), loopTime, std::placeholders::_1);
	scheduler->scheduleAt(std::move(schedTask), time);
}

void Scheduler::wakeUp() {
	{
		std::unique_lock<std::mutex> lock(mutex);
		nextWakeUpTime = NEVER;
	}

	// assume the time is fixed for the time of this call:
	// we don't wait anywhere.
	auto const now = clock->now();

	auto expiredTasks = advanceTime(now);

	// run expired tasks: must be done asynchronously
	for(auto& t : expiredTasks) {
		t.task(t.time);
	}

	reschedule();
}

std::vector<Scheduler::Task> Scheduler::advanceTime(Fraction now) {
	std::vector<Task> expiredTasks;

	std::unique_lock<std::mutex> lock(mutex);

	// collect all expired tasks
	while(!queue.empty() && queue.top().time <= now) {
		expiredTasks.emplace_back(std::move(queue.top()));
		queue.pop();
	}

	return expiredTasks;
}

void Scheduler::reschedule() {
	std::unique_lock<std::mutex> lock(mutex);

	if(queue.empty())
		return;

	auto const topTime = queue.top().time;

	// set the next wake-up time, if any
	if(topTime < nextWakeUpTime || nextWakeUpTime == NEVER) {
		nextWakeUpTime = topTime;
		auto runDg = std::bind(&Scheduler::wakeUp, this);
		timer->scheduleIn(runDg, topTime - clock->now());
	}
}

