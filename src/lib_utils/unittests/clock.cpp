#include <iostream>
#include "tests/tests.hpp"
#include "lib_utils/sysclock.hpp"
#include "lib_utils/system_clock.hpp"

using namespace Tests;

namespace {

auto const f20 = Fraction(20, 1000);

secondclasstest("global clock") {
	for (int i = 0; i < 5; ++i) {
		auto const now = (double)g_SystemClock->now();
		std::cout << "Time: " << now << std::endl;
		g_SystemClock->sleep(f20);
	}
}

secondclasstest("basic clock, speed 0.5x") {
	SystemClock clock(0.5);
	for (int i = 0; i < 5; ++i) {
		auto const now = (double)g_SystemClock->now();
		std::cout << "Time: " << now << std::endl;
		g_SystemClock->sleep(f20);
	}
}

secondclasstest("basic clock, speed 2x") {
	SystemClock clock(2.0);
	for (int i = 0; i < 5; ++i) {
		auto const now = (double)g_SystemClock->now();
		std::cout << "Time: " << now << std::endl;
		g_SystemClock->sleep(f20);
	}
}

}
