#include "tests/tests.hpp"
#include "lib_signals/signals.hpp"
#include "lib_utils/tools.hpp" // makeVector
#include "lib_utils/queue_inspect.hpp"

using namespace Tests;
using namespace Signals;

namespace {

std::vector<int> results;

inline void dummy(int a) {
	results.push_back(a);
}
void dummy2(int a) {
	results.push_back(1 + a);
}

unittest("signals_simple: disconnect non existing") {
	Signal<void(int)> sig;
	ASSERT(!sig.disconnect(0));
}

unittest("signals_simple") {
	Signal<void(int)> sig;

	auto const id = sig.connect(dummy);

	results.clear();
	sig.emit(100);
	ASSERT_EQUALS(makeVector({100}), results);

	auto id2 = sig.connect(dummy2);
	sig.connect(dummy);
	sig.connect(dummy2);
	results.clear();
	sig.emit(777);

	auto expected = makeVector({777, 778, 777, 778});
	ASSERT_EQUALS(expected, results);

	ASSERT(sig.getNumConnections() == 4);

	{
		bool res;
		res = sig.disconnect(id2);
		ASSERT(res);

		res = sig.disconnect(id);
		ASSERT(res);

		//disconnect again
		res = sig.disconnect(id);
		ASSERT(!res);

		//wrong id
		res = sig.disconnect(id + 1);
		ASSERT(!res);
	}
}

unittest("signals_simple (void return value)") {
	Signal<void(int)> sig;
	sig.connect(dummy);
	sig.emit(100);
}

unittest("connect to lambda") {
	int result = 0;
	Signal<void(int)> sig;
	Connect(sig, [&](int val) {
		result = val * val;
	});
	sig.emit(8);
	ASSERT_EQUALS(64, result);
}
}
