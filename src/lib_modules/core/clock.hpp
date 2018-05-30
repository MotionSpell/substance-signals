#pragma once

#include "lib_utils/clock.hpp"
#include <memory>

class IClockCap {
	public:
		virtual ~IClockCap() noexcept(false) {}

	protected:
		/*FIXME: we need to have factories to move these back to the implementation - otherwise ports created from the constructor may crash*/
		std::shared_ptr<IClock> clock;
};

class ClockCap : public virtual IClockCap {
	public:
		ClockCap(const std::shared_ptr<IClock> clock) {
			this->clock = clock;
		}
};
