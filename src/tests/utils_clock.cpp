#include <iostream>
#include "tests.hpp"
#include "lib_utils/clock.hpp"

using namespace Tests;

namespace {

auto const f20 = Fraction(20, 1000);

unittest("global clock") {
	for (int i = 0; i < 5; ++i) {
		auto const now = (double)g_DefaultClock->now();
		std::cout << "Time: " << now << std::endl;
		g_DefaultClock->sleep(f20);
	}
}

unittest("basic clock, speed 0.5x") {
	Clock clock(0.5);
	for (int i = 0; i < 5; ++i) {
		auto const now = (double)g_DefaultClock->now();
		std::cout << "Time: " << now << std::endl;
		g_DefaultClock->sleep(f20);
	}
}

unittest("basic clock, speed 2x") {
	Clock clock(2.0);
	for (int i = 0; i < 5; ++i) {
		auto const now = (double)g_DefaultClock->now();
		std::cout << "Time: " << now << std::endl;
		g_DefaultClock->sleep(f20);
	}
}

}
