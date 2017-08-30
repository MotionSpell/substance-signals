#include "scheduler.hpp"
#include "log.hpp"

Scheduler::Scheduler(std::shared_ptr<IClock> clock) : waitAndExit(false), clock(clock) {
	schedThread = std::thread(&Scheduler::threadProc, this);
}

Scheduler::~Scheduler() {
	waitAndExit = true;
	{
		std::unique_lock<std::mutex> lock(mutex);
		condition.notify_one();
	}
	if (schedThread.joinable()) {
		schedThread.join();
	}
}

void Scheduler::scheduleAt(const std::function<void(Fraction)> &&task, Fraction timeUTC) {
	std::unique_lock<std::mutex> lock(mutex);
	queue.push(uptr(new Task(std::move(task), timeUTC)));
	condition.notify_one();
}

void Scheduler::scheduleEvery(const std::function<void(Fraction)> &&task, Fraction loopTime, Fraction startTimeUTC) {
	if (startTimeUTC == 0) {
		startTimeUTC = Fraction(clock->now(1000), 1000);
	}
	const std::function<void(Fraction)> schedTask = [&, task2{ std::move(task) }, loopTime](Fraction startTimeUTC) {
		task2(startTimeUTC);
		scheduleEvery(std::move(task2), loopTime, startTimeUTC + loopTime);
	};
	scheduleAt(std::move(schedTask), startTimeUTC);
}

void Scheduler::threadProc() {
	while (!waitAndExit) {
		{
			std::unique_lock<std::mutex> lock(mutex);
			if (queue.empty()) {
				condition.wait(lock);
				continue;
			}
		}

		if (clock->getSpeed()) {
			auto &t = queue.top();
			auto const waitDurInMs = (int64_t)(t->time - clock->now());
			if (waitDurInMs < 0) {
				Log::msg(Warning, "Late from %s ms.", -waitDurInMs);
			} else if (waitDurInMs > 0) {
				std::unique_lock<std::mutex> lock(mutex);
				auto const durInMs = std::chrono::milliseconds((int64_t)(waitDurInMs / clock->getSpeed()));
				if (condition.wait_for(lock, durInMs, [&] { return (queue.top()->time < t->time) || waitAndExit; })) {
					continue;
				}
			}
		}

		{
			mutex.lock();
			auto const &t = queue.top();
			mutex.unlock();
			t->task(t->time); //TODO: tasks may be blocking so we might want to create a pool instead of a single thread
			mutex.lock();
			queue.pop();
			mutex.unlock();
		}
	}
}
