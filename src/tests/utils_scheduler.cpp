#include "tests.hpp"
#include "lib_utils/scheduler.hpp"

namespace {

auto const f1  = Fraction( 1, 1000);
auto const f10 = Fraction(10, 1000);
auto const f50 = Fraction(50, 1000);
auto const f1000 = Fraction(1, 1);

unittest("scheduler: basic, scheduleIn()/scheduleAt()") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};
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
		s.scheduleIn(f, f1000);
		clock->sleep(50, 1000);
	}
	ASSERT(q.size() == 1);
	q.clear();

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleIn(f, 0);
		s.scheduleIn(f, f1);
		clock->sleep(50, 1000);
	}
	ASSERT(q.size() == 2);
	auto const t1 = q.pop(), t2 = q.pop();
	ASSERT(t2 - t1 == f1);
}

unittest("scheduler: scheduleEvery()") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};
	auto const f10 = Fraction(10, 1000);

	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleEvery(f, f10, 0);
		clock->sleep(50, 1000);
	}
	ASSERT(q.size() >= 3);
	auto const t1 = q.pop(), t2 = q.pop();
	ASSERT(t2 - t1 == f10);
}

unittest("scheduler: reschedule a sooner event while waiting") {
	Queue<Fraction> q;
	{
		auto clock = shptr(new Clock(1.0));
		Scheduler s(clock);
		s.scheduleIn([&](Fraction f) { q.push(f); }, f10);
		s.scheduleIn([&](Fraction f) { q.push(f); }, 0);
		clock->sleep(50, 1000);
	}
	std::cout << q.size() << std::endl;
	ASSERT(q.size() == 2);
}

}
