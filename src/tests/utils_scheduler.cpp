#include "tests.hpp"
#include "lib_utils/scheduler.hpp"

namespace {

unittest("scheduler: basic, scheduleIn()/scheduleAt()") {
	Queue<int> q;
	std::atomic_int i(0);
	auto f = [&] {
		q.push(i++);
	};
	auto clear = [&] { q.clear(); i = 0; };

	{
		Scheduler s;
	}

	{
		Scheduler s;
		s.scheduleIn(f, 50);
	}
	ASSERT(i == 0);
	clear();

	{
		Scheduler s;
		s.scheduleIn(f, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	ASSERT(i == 1);
	clear();

	{
		Scheduler s;
		s.scheduleIn(f, 0);
		s.scheduleIn(f, 1000);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	ASSERT(i == 1);
	clear();

	{
		Scheduler s;
		s.scheduleIn(f, 0);
		s.scheduleIn(f, 1);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	ASSERT(i == 2);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 1);
}

unittest("scheduler: scheduleEvery()") {
	Queue<int> q;
	std::atomic_int i(0);
	auto f = [&] {
		q.push(i++);
	};

	{
		Scheduler s;
		s.scheduleEvery(f, getUTCInMs(), 10);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	ASSERT(i >= 3);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 1);
	ASSERT(q.pop() == 2);
}

#ifdef ENABLE_FAILING_TESTS
unittest("scheduler: scheduleEvery()") {
	//TODO: non void(void)
}
#endif

unittest("scheduler: reschedule a sooner event while waiting") {
	Queue<int> q;
	std::atomic_int i(0);

	{
		Scheduler s;
		s.scheduleIn([&] { q.push(20); }, 20);
		s.scheduleIn([&] { q.push(0); }, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	ASSERT(q.size() == 2);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 20);
}

}
