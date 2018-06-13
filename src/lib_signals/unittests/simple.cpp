#include "tests/tests.hpp"
#include "lib_signals/signals.hpp"
#include "lib_utils/queue_inspect.hpp"

using namespace Tests;
using namespace Signals;

namespace {
inline int dummy(int a) {
	return a;
}
int dummy2(int a) {
	return dummy(1 + dummy(a));
}

unittest("signals_simple: disconnect non existing") {
	Signal<int(int)> sig;
	ASSERT(!sig.disconnect(0));
}

unittest("signals_simple") {
	Signal<int(int)> sig;

	size_t id = sig.connect(dummy);

	auto numVal = sig.emit(100);
	auto val = sig.results();
	ASSERT_EQUALS(makeVector({100}), transferToVector(*val));

	size_t id2 = sig.connect(dummy2);
	sig.connect(dummy);
	sig.connect(dummy2);
	numVal = sig.emit(777);
	val = sig.results();
	ASSERT_EQUALS(4u, numVal);

	auto expected = makeVector({
		dummy(777),
		dummy2(777),
		dummy(777),
		dummy2(777)});
	ASSERT_EQUALS(expected, transferToVector(*val));

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
	sig.results();
}

unittest("connect to lambda") {
	Signal<int(int)> sig;
	Connect(sig, [](int val) -> int { return val * val; });
	sig.emit(8);
	auto const res = sig.results();
	ASSERT_EQUALS(makeVector({64}), transferToVector(*res));
}
}
