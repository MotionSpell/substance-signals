#include <iostream>
#include "tests.hpp"
#include "lib_signals/signals.hpp"
#include "lib_utils/tools.hpp"

//#define ENABLE_FAILING_TESTS
//#define ENABLE_PERF_TESTS

namespace {
inline int dummy(int a) {
	return a;
}
inline int dummyPrint(int a) {
	std::cout << "a = " << a << std::endl;
	return a;
}
inline void sleepInMs(int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
}

#include "signals_unit_result.cpp"
#include "signals_simple.cpp"
#include "signals_module.cpp"
#include "signals_async.cpp"
#ifdef ENABLE_PERF_TESTS
#include "signals_perf.cpp"
#endif

using namespace Tests;
