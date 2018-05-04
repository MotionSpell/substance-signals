#include "tests/tests.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/tools.hpp" // Fraction
#include "lib_utils/scheduler.hpp"
#include "lib_utils/sysclock.hpp"

namespace {

auto const f0  = Fraction( 0, 1000);
auto const f1  = Fraction( 1, 1000);
auto const f10 = Fraction(10, 1000);
auto const f50 = Fraction(50, 1000);
auto const f1000 = Fraction(1, 1);
const double clockSpeed = 1.0;

unittest("scheduler: basic)") {
	Scheduler s(shptr(new Clock(clockSpeed)));
}

unittest("scheduler: scheduleIn 1") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};

	Scheduler s(shptr(new Clock(clockSpeed)));
	s.scheduleIn(f, f50);
	ASSERT(q.size() == 0);
}

unittest("scheduler: scheduleIn 2") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};

	auto clock = shptr(new Clock(clockSpeed));
	Scheduler s(clock);
	s.scheduleIn(f, 0);
	clock->sleep(f50);
	ASSERT(q.size() == 1);
}

unittest("scheduler: scheduleIn 3") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};

	auto clock = shptr(new Clock(clockSpeed));
	Scheduler s(clock);
	s.scheduleIn(f, 0);
	s.scheduleIn(f, f1000);
	clock->sleep(f50);
	ASSERT(q.size() == 1);
}

unittest("scheduler: scheduleAt") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};

	auto clock = shptr(new Clock(clockSpeed));
	Scheduler s(clock);
	auto const now = clock->now();
	s.scheduleAt(f, now + f0);
	s.scheduleAt(f, now + f1);
	clock->sleep(f50);
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
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleEvery(f, f10, 0);
		clock->sleep(f50);
	}
	ASSERT(q.size() >= 3);
	auto const t1 = q.pop(), t2 = q.pop();
	ASSERT(t2 - t1 == f10);
}

unittest("scheduler: reschedule a sooner event while waiting") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleIn(f, f10);
		s.scheduleIn(f, 0);
		clock->sleep(f50);
	}
	ASSERT(q.size() == 2);
}

}
