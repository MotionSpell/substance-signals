#include "tests.hpp"
#include "lib_utils/scheduler.hpp"

namespace {

unittest("scheduler: basic, scheduleIn()/scheduleAt()") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};
	auto const f50 = Fraction(50, 1000);
	const double clockSpeed = 1.0; //Romain: test with other values

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
	}

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleIn(f, f50);
	}
	ASSERT(q.size() == 0);
	q.clear();

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleIn(f, 0);
		clock->sleep(50, 1000); //Romain: use Fractions
	}
	ASSERT(q.size() == 1);
	q.clear();

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleIn(f, 0);
		s.scheduleIn(f, 1000);
		clock->sleep(50, 1000);
	}
	ASSERT(q.size() == 1);
	q.clear();

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleIn(f, 0);
		s.scheduleIn(f, 1);
		clock->sleep(50, 1000);
	}
	ASSERT(q.size() == 2);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 1);
}

unittest("scheduler: scheduleEvery()") {
	Queue<int> q;
	std::atomic_int i(0);
	auto f = [&](Fraction) {
		q.push(i++);
	};

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleEvery(f, 10, clock->now());
		clock->sleep(50, 1000);
	}
	ASSERT(i >= 3);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 1);
	ASSERT(q.pop() == 2);
}

unittest("scheduler: reschedule a sooner event while waiting") {
	Queue<int> q;
	std::atomic_int i(0);

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleIn([&](Fraction) { q.push(20); }, 20);
		s.scheduleIn([&](Fraction) { q.push(0); }, 0);
		clock->sleep(50, 1000);
	}
	ASSERT(q.size() == 2);
	ASSERT(q.pop() == 0);
	ASSERT(q.pop() == 20);
}

}
