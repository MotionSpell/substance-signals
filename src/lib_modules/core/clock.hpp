#pragma once

#include "lib_utils/clock.hpp"
#include <memory>

class IClockCap {
public:
	virtual ~IClockCap() noexcept(false) {}
	virtual const std::shared_ptr<IClock> getClock() const = 0; //FIXME: unused but otherwise class wouldn't be abstract

protected:
	/*FIXME: we need to have factories to move these back to the implementation - otherwise pins created from the constructor may crash*/
	std::shared_ptr<IClock> clock;
};

class ClockCap : public virtual IClockCap {
public:
	ClockCap(const std::shared_ptr<IClock> clock) {
		this->clock = clock;
	}

protected:
	const std::shared_ptr<IClock> getClock() const override {
		return clock;
	}
};
