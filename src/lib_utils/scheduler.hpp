#pragma once

#include "clock.hpp"
#include "time.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

struct IScheduler {
	virtual ~IScheduler() {}
	virtual void scheduleAt(const std::function<void(void)> &&task, uint64_t absTimeInMs) = 0;
	virtual void scheduleIn(const std::function<void(void)> &&task, uint64_t timeInMs) {
		scheduleAt(std::move(task), getUTCInMs() + timeInMs);
	}
	virtual void scheduleEvery(const std::function<void(void)> &&task, uint64_t startTimeInMs, uint64_t loopTimeInMs/*TODO: , uint64_t stopTimeInMs = -1*/) = 0;
	//TODO: scheduleWhen() would allow to remove threads in modules, see https://github.com/gpac/signals/issues/14 + remark below on the necessity of thread pool + should be attached to any thread (e.g. th0).
};

class Scheduler : public IScheduler {
public:
	Scheduler(std::shared_ptr<IClock> clock = g_DefaultClock);
	~Scheduler();
	void scheduleAt(const std::function<void(void)> &&task, uint64_t absTimeUTCInMs) override;
	void scheduleEvery(const std::function<void(void)> &&task, uint64_t startTimeUTCInMs, uint64_t loopTimeInMs) override;

private:
	struct Task {
		struct Sooner {
			bool operator()(const std::unique_ptr<Task> &lhs, const std::unique_ptr<Task> &rhs) const {
				return lhs->absTimeUTCInMs > rhs->absTimeUTCInMs;
			}
		};
		Task(const std::function<void(void)> &&task2, uint64_t absTimeUTCInMs) : task(std::move(task2)), absTimeUTCInMs(absTimeUTCInMs) {
		}
		std::function<void(void)> task;
		uint64_t absTimeUTCInMs;
	};

	void threadProc();

	std::mutex mutex;
	std::condition_variable condition;
	std::priority_queue<std::unique_ptr<Task>, std::deque<std::unique_ptr<Task>>, Task::Sooner> queue;
	std::atomic_bool waitAndExit;
	std::thread schedThread;
	std::shared_ptr<IClock> clock;
};
