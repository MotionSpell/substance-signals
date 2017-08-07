#pragma once

#include "log.hpp"
#include "time.hpp"
#include "tools.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

struct IScheduler {
	virtual void scheduleAt(const std::function<void(void)> &&task, uint64_t absTimeInMs) = 0; //TODO: not void(void)...
	virtual void scheduleIn(const std::function<void(void)> &&task, uint64_t timeInMs) {
		scheduleAt(std::move(task), getUTCInMs() + timeInMs);
	}
	virtual void scheduleEvery(const std::function<void(void)> &&task, uint64_t startTimeInMs, uint64_t loopTimeInMs/*TODO: , uint64_t stopTimeInMs = -1*/) = 0;
	//TODO: scheduleWhen() would allow to remove threads in modules, see https://github.com/gpac/signals/issues/14 + remark below on the necessity of thread pool + should be attached to any thread (e.g. th0).
};

//one thread + using the system clock
class Scheduler : public IScheduler {
public:
	Scheduler() : waitAndExit(false) {
		schedThread = std::thread(&Scheduler::threadProc, this);
	}
	~Scheduler() {
		waitAndExit = true;
		{
			std::unique_lock<std::mutex> lock(mutex);
			condition.notify_one();
		}
		if (schedThread.joinable()) {
			schedThread.join();
		}
	}
	void scheduleAt(const std::function<void(void)> &&task, uint64_t absTimeUTCInMs) override {
		std::unique_lock<std::mutex> lock(mutex);
		queue.push(uptr(new Task(std::move(task), absTimeUTCInMs)));
		condition.notify_one();
	}
	void scheduleEvery(const std::function<void(void)> &&task, uint64_t startTimeUTCInMs, uint64_t loopTimeInMs) override {
		const std::function<void(void)> schedTask = [&, task2{ std::move(task) }, startTimeUTCInMs, loopTimeInMs] {
			task2();
			scheduleEvery(std::move(task2), startTimeUTCInMs + loopTimeInMs, loopTimeInMs);
		};
		scheduleAt(std::move(schedTask), startTimeUTCInMs);
	}

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

	void threadProc() {
		while (!waitAndExit) {
			{
				std::unique_lock<std::mutex> lock(mutex);
				if (queue.empty()) {
					condition.wait(lock);
					continue;
				}
			}

			auto &t = queue.top();
			auto const waitDurInMs = (int64_t)(t->absTimeUTCInMs - getUTCInMs());
			if (waitDurInMs < 0) {
				Log::msg(Warning, "Late from %s ms.", -waitDurInMs);
			} else if (waitDurInMs > 0) {
				std::unique_lock<std::mutex> lock(mutex);
				if (condition.wait_for(lock, std::chrono::milliseconds(waitDurInMs), [&] { return (queue.top()->absTimeUTCInMs < t->absTimeUTCInMs) || waitAndExit; })) {
					continue;
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

	std::mutex mutex;
	std::condition_variable condition;
	std::priority_queue<std::unique_ptr<Task>, std::deque<std::unique_ptr<Task>>, Task::Sooner> queue;
	std::atomic_bool waitAndExit;
	std::thread schedThread;
};
