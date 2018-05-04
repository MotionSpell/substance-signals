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
	schedThread.join();
}

void Scheduler::scheduleAt(const std::function<void(Fraction)> &&task, Fraction time) {
	std::unique_lock<std::mutex> lock(mutex);
	queue.push(Task(std::move(task), time));
	condition.notify_one();
}

void Scheduler::scheduleEvery(const std::function<void(Fraction)> &&task, Fraction loopTime, Fraction time) {
	const std::function<void(Fraction)> schedTask = [&, task2(std::move(task)), loopTime](Fraction startTime) {
		task2(startTime);
		scheduleEvery(std::move(task2), loopTime, startTime + loopTime);
	};
	scheduleAt(std::move(schedTask), time);
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

		{
			auto &t = queue.top();
			auto const waitDur = t.time - clock->now();
			if (clock->getSpeed()) {
				auto const waitDurInMs = 1000 * (double)(waitDur);
				if (waitDurInMs < 0) {
					Log::msg(Warning, "Late from %s ms.", -waitDurInMs);
				} else if (waitDurInMs > 0) {
					std::unique_lock<std::mutex> lock(mutex);
					auto const durInMs = std::chrono::milliseconds((int64_t)(waitDurInMs / clock->getSpeed()));
					if (condition.wait_for(lock, durInMs, [&] { return (queue.top().time < t.time) || waitAndExit; })) {
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
