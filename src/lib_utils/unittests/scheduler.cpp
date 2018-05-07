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

unittest("scheduler: basic") {
	Scheduler s(shptr(new Clock(clockSpeed)));
}

unittest("scheduler: scheduled events are delayed") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};

	Scheduler s(shptr(new Clock(clockSpeed)));
	s.scheduleIn(f, f50);
	ASSERT(transferToVector(q).empty());
}

unittest("scheduler: scheduled events are not delayed too much") {
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

unittest("scheduler: expired scheduled events are executed, but not the others") {
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

unittest("scheduler: absolute-time scheduled events are received in order") {
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

unittest("scheduler: periodic events are executed periodically") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};
	auto const period = Fraction(10, 1000);

	{
		auto clock = shptr(new Clock(clockSpeed));
		Scheduler s(clock);
		s.scheduleEvery(f, period, 0);
		clock->sleep(f50);
	}
	auto v = transferToVector(q);
	ASSERT(v.size() >= 3);
	auto const t1 = v[0], t2 = v[1];
	ASSERT_EQUALS(period, t2 - t1);
}

unittest("scheduler: events scheduled out-of-order are executed in order") {
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
	ASSERT_EQUALS(makeVector({Fraction(0), f10}), v);
}

unittest("[disabled] scheduler: can still schedule and trigger 'near' tasks while waiting for a 'far' one") {
	return;
	auto const oneMsec = Fraction(1, 1000);
	auto const oneHour = Fraction(3600, 1);

	auto clock = shptr(new Clock(clockSpeed));
	Queue<Fraction> q;
	auto f = [&](Fraction /*time*/) {
		q.push(clock->now());
	};

	Scheduler s(clock);
	s.scheduleIn(f, oneHour);
	clock->sleep(f10); // let the scheduler run and start waiting for oneHour
	s.scheduleIn(f, oneMsec); // now schedule an imminent task
	clock->sleep(f10 * 3); // allow some time for the imminent task to run

	// don't destroy 's' now, as it would wake it up and miss the goal of this test

	auto v = transferToVector(q);
	ASSERT_EQUALS(1u, v.size());
}

}
