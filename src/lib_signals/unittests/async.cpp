#include "tests/tests.hpp"
#include "lib_signals/signals.hpp"
#include "lib_utils/queue_inspect.hpp" // transferToVector

using namespace Tests;
using namespace Signals;

namespace {

inline int dummy(int a) {
	return a;
}
inline void sleepInMs(int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
int sleepAndDummy(int ms, int a) {
	sleepInMs(ms);
	return a;
}

unittest("destroy on execution") {
	Signal<void(int)> sig;
	sig.connect(sleepInMs);
	sig.emit(1000);
}

unittest("disconnect before execution") {
	Signal<void(int)> sig;
	size_t uid = sig.connect(sleepInMs);
	sig.disconnect(uid);
	sig.emit(1000);
}

unittest("disconnect on execution") {
	Signal<void(int)> sig;
	size_t uid = sig.connect(sleepInMs);
	sig.emit(1000);
	sig.disconnect(uid);
}

unittest("as many results as emit() calls") {
	Signal<int(int)> sig;
	sig.connect(dummy);
	sig.emit(27);
	sig.emit(1789);
	auto res = sig.results();
	ASSERT_EQUALS(makeVector({27, 1789}), transferToVector(*res));
}

unittest("as many results as emit() calls, results arriving in wrong order") {
	Signal<int(int, int)> sig;
	sig.connect(sleepAndDummy);
	sig.emit(200, 27);
	sig.emit(20, 1789);
	auto res = sig.results();
	ASSERT_EQUALS(makeVector({27, 1789}), transferToVector(*res));
}

unittest("as many results as emit() calls, results arriving in wrong order") {
	Signal<int(int, int)> sig;
	sig.connect(sleepAndDummy);
	sig.emit(200, 27);
	sig.emit(20, 1789);
	auto res = sig.results();
	ASSERT_EQUALS(makeVector({27, 1789}), transferToVector(*res));
}
}
