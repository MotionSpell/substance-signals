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
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
	}

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleIn(f, 50);
	}
	ASSERT(i == 0);
	clear();

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleIn(f, 0);
		clock->sleep(50, 1000);
	}
	ASSERT(i == 1);
	clear();

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleIn(f, 0);
		s.scheduleIn(f, 1000);
		clock->sleep(50, 1000);
	}
	ASSERT(i == 1);
	clear();

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleIn(f, 0);
		s.scheduleIn(f, 1);
		clock->sleep(50, 1000);
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
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleEvery(f, 10, getUTC());
		clock->sleep(50, 1000);
	}
	ASSERT(i >= 3);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 1);
	ASSERT(q.pop() == 2);
}

#ifdef ENABLE_FAILING_TESTS
unittest("scheduler: scheduleEvery() on non void types") {
	//TODO: non void(void)
}
#endif

unittest("scheduler: reschedule a sooner event while waiting") {
	Queue<int> q;
	std::atomic_int i(0);

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleIn([&] { q.push(20); }, 20);
		s.scheduleIn([&] { q.push(0); }, 0);
		clock->sleep(50, 1000);
	}
	ASSERT(q.size() == 2);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 20);
}

}
