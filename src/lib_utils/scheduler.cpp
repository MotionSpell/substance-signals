#include "scheduler.hpp"
#include "log.hpp"

Scheduler::Scheduler(std::shared_ptr<IClock> clock) : clock(clock) {
	schedThread = std::thread(&Scheduler::threadProc, this);
}

Scheduler::~Scheduler() {
	{
		std::unique_lock<std::mutex> lock(mutex);
		stopThread = true;
	}
	condition.notify_one();
	schedThread.join();
}

void Scheduler::scheduleAt(TaskFunc &&task, Fraction time) {
	std::unique_lock<std::mutex> lock(mutex);
	queue.push(Task(std::move(task), time));
	condition.notify_one();
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

void Scheduler::threadProc() {

	auto wakeUpCondition = [&]() {
		return stopThread || !queue.empty();
	};

	while (1) {
		{
			std::unique_lock<std::mutex> lock(mutex);
			condition.wait(lock, wakeUpCondition);
			if(stopThread)
				break;
		}

		{
			auto const waitDur = waitDuration();
			if (clock->getSpeed()) {
				auto const waitDurInMs = 1000 * (double)(waitDur);
				if (waitDurInMs < 0) {
					Log::msg(Warning, "Late from %s ms.", -waitDurInMs);
				} else if (waitDurInMs > 0) {
					std::unique_lock<std::mutex> lock(mutex);
					auto const durInMs = std::chrono::milliseconds((int64_t)(waitDurInMs / clock->getSpeed()));
					if (condition.wait_for(lock, durInMs, [&] { return waitDuration() < 0 || stopThread; })) {
						continue;
					}
				}
			} else {
				clock->sleep(waitDur);
			}
		}

		{
			Task t(nullptr, Fraction(0,0));

			{
				std::unique_lock<std::mutex> lock(mutex);
				t = std::move(queue.top());
				queue.pop();
			}

			t.task(t.time);
		}
	}
}

Fraction Scheduler::waitDuration() const {
	return queue.top().time - clock->now();
}
