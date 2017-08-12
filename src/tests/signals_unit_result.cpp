#include "tests.hpp"
#include "lib_signals/signals.hpp"
#include "lib_signals/core/result.hpp"

using namespace Tests;
using namespace Signals;

namespace {
template<typename T>
bool test() {
	ResultQueue<T> result;
	auto res = result.get();

	return true;
}

unittest("unit test on class Result") {
	{
		const bool res = test<int>();
		ASSERT(res);
	}
	{
		const bool res = test<void>();
		ASSERT(res);
	}
}
}
