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

void Scheduler::scheduleAt(const std::function<void(void)> &&task, uint64_t absTimeUTCInMs) {
	std::unique_lock<std::mutex> lock(mutex);
	queue.push(uptr(new Task(std::move(task), absTimeUTCInMs)));
	condition.notify_one();
}

void Scheduler::scheduleEvery(const std::function<void(void)> &&task, uint64_t startTimeUTCInMs, uint64_t loopTimeInMs) {
	const std::function<void(void)> schedTask = [&, task2{ std::move(task) }, startTimeUTCInMs, loopTimeInMs] {
		task2();
		scheduleEvery(std::move(task2), startTimeUTCInMs + loopTimeInMs, loopTimeInMs);
	};
	scheduleAt(std::move(schedTask), startTimeUTCInMs);
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
			auto const waitDurInMs = (int64_t)(t->absTimeUTCInMs - getUTCInMs());
			if (waitDurInMs < 0) {
				Log::msg(Warning, "Late from %s ms.", -waitDurInMs);
			}
			else if (waitDurInMs > 0) {
				std::unique_lock<std::mutex> lock(mutex);
				if (condition.wait_for(lock, std::chrono::milliseconds((int64_t)(waitDurInMs / clock->getSpeed())), [&] { return (queue.top()->absTimeUTCInMs < t->absTimeUTCInMs) || waitAndExit; })) {
					continue;
				}
			}
		}

		{
			mutex.lock();
			auto &t = queue.top();
			mutex.unlock();
			t->task(); //TODO: tasks may be blocking so we might want to create a pool instead of a single thread
			mutex.lock();
			queue.pop();
			mutex.unlock();
		}
	}
}
