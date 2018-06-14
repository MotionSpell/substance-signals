#pragma once

#include "i_scheduler.hpp"

#include "clock.hpp"
#include "lib_utils/default_clock.hpp"
#include "time.hpp"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

class Scheduler : public IScheduler {
	public:
		Scheduler(std::shared_ptr<IClock> clock = g_DefaultClock);
		~Scheduler();
		void scheduleAt(TaskFunc &&task, Fraction time) override;
		void scheduleIn(TaskFunc &&task, Fraction time) override {
			scheduleAt(std::move(task), clock->now() + time);
		}

	private:
		struct Task {
			bool operator<(const Task &other) const {
				return time > other.time;
			}
			Task(const TaskFunc &&task2, Fraction time)
				: task(std::move(task2)), time(time) {
			}
			TaskFunc task;
			Fraction time;
		};

		void threadProc();

		Fraction waitDuration() const;

		std::mutex mutex;
		std::condition_variable condition;
		std::priority_queue<Task, std::deque<Task>> queue;
		bool stopThread = false;
		std::thread schedThread;
		std::shared_ptr<IClock> clock;
};
