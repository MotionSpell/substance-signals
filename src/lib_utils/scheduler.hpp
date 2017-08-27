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
	virtual void scheduleAt(const std::function<void(void)> &&task, Fraction timeUTC) = 0;
	virtual void scheduleIn(const std::function<void(void)> &&task, Fraction timeUTC) {
		scheduleAt(std::move(task), getUTC() + timeUTC);
	}
	virtual void scheduleEvery(const std::function<void(void)> &&task, Fraction loopTime, Fraction startTimeUTC = 0/*TODO: , Fraction stopTimeUTC = -1*/) = 0;
	//TODO: scheduleWhen() would allow to remove threads in modules, see https://github.com/gpac/signals/issues/14 + remark below on the necessity of thread pool + should be attached to any thread (e.g. th0).
};

class Scheduler : public IScheduler {
public:
	Scheduler(std::shared_ptr<IClock> clock = g_DefaultClock);
	~Scheduler();
	void scheduleAt(const std::function<void(void)> &&task, Fraction timeUTC) override;
	void scheduleEvery(const std::function<void(void)> &&task, Fraction loopTime, Fraction startTimeUTC) override;

private:
	struct Task {
		struct Sooner {
			bool operator()(const std::unique_ptr<Task> &lhs, const std::unique_ptr<Task> &rhs) const {
				return lhs->timeUTC > rhs->timeUTC;
			}
		};
		Task(const std::function<void(void)> &&task2, Fraction timeUTC) : task(std::move(task2)), timeUTC(timeUTC) {
		}
		std::function<void(void)> task;
		Fraction timeUTC;
	};

	void threadProc();

	std::mutex mutex;
	std::condition_variable condition;
	std::priority_queue<std::unique_ptr<Task>, std::deque<std::unique_ptr<Task>>, Task::Sooner> queue;
	std::atomic_bool waitAndExit;
	std::thread schedThread;
	std::shared_ptr<IClock> clock;
};
