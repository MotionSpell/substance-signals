#pragma once

#include "clock.hpp"
#include <chrono>

class SystemClock : public IClock {
	public:
		SystemClock(double speed);
		Fraction now() const override;
		void sleep(Fraction time) const override;

	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> const timeStart;
		double const speed;
};

