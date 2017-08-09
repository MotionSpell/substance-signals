#pragma once

#include "lib_utils/clock.hpp"
#include <memory>

struct IClockCap {
	virtual void setClock(const std::shared_ptr<IClock> clock) = 0;
};

class ClockCap : public virtual IClockCap {
protected:
	void setClock(const std::shared_ptr<IClock> clock) override {
		this->clock = clock;
	}
	std::shared_ptr<IClock> clock;
};
