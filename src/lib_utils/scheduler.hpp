#pragma once

#include "i_scheduler.hpp"

#include "clock.hpp"
#include "lib_utils/default_clock.hpp"
#include "time.hpp"
#include "timer.hpp"
#include <mutex>
#include <queue>

class Scheduler : public IScheduler {
	public:
		Scheduler(std::shared_ptr<IClock> clock = g_DefaultClock, std::shared_ptr<ITimer> timer = std::shared_ptr<ITimer>(new SystemTimer));
		void scheduleAt(TaskFunc &&task, Fraction time) override;
		void scheduleIn(TaskFunc &&task, Fraction time) override {
			scheduleAt(std::move(task), clock->now() + time);
		}

	private:
		// checks if there's anything to do, and do it.
		// returns immediately. Always run from the timer thread.
		void wakeUp();

		void reschedule();

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

		// removes from 'queue' the list of expired tasks
		std::vector<Task> advanceTime(Fraction time);

		std::mutex mutex; // protects 'queue'
		std::priority_queue<Task, std::deque<Task>> queue;

		std::shared_ptr<ITimer> timer;
		std::shared_ptr<IClock> clock;

		Fraction nextWakeUpTime;
};

