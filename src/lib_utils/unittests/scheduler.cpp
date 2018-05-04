#include "tests/tests.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/queue_inspect.hpp"
#include "lib_utils/fraction.hpp"
#include "lib_utils/tools.hpp" // shptr
#include "lib_utils/scheduler.hpp"
#include "lib_utils/sysclock.hpp"

// allows ASSERT_EQUALS on fractions
static std::ostream& operator<<(std::ostream& o, Fraction f) {
	o << f.num << "/" << f.den;
	return o;
}

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
	ASSERT(transferToVector(q).empty());
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
	ASSERT_EQUALS(1u, transferToVector(q).size());
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
	ASSERT_EQUALS(1u, transferToVector(q).size());
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
	auto v = transferToVector(q);
	ASSERT_EQUALS(2u, v.size());
	auto const t1 = v[0], t2 = v[1];
	ASSERT_EQUALS(f1, t2 - t1);
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
	auto v = transferToVector(q);
	ASSERT(v.size() >= 3);
	auto const t1 = v[0], t2 = v[1];
	ASSERT_EQUALS(f10, t2 - t1);
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
	auto v = transferToVector(q);
	ASSERT_EQUALS(2u, v.size());
}

}
