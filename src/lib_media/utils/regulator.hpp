#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {

class Regulator : public ModuleS {
	public:
		Regulator(std::shared_ptr<IClock> clock_) : clock(clock_) {
			createInput(this);
			addOutput<OutputDefault>();
		}

		void process(Data data) override {
			auto const dataTime = data->getMediaTime();
			if (clock->getSpeed() > 0.0) {
				auto const delayInMs = clockToTimescale(dataTime - fractionToClock(clock->now()), 1000);
				if (delayInMs > 0) {
					Log::msg(delayInMs < REGULATION_TOLERANCE_IN_MS ? Debug : Warning, "received data for time %ss (will sleep for %s ms)", dataTime / (double)IClock::Rate, delayInMs);
					clock->sleep(Fraction(delayInMs, 1000));
				} else if (delayInMs + REGULATION_TOLERANCE_IN_MS < 0) {
					Log::msg(dataTime > 0 ? Warning : Debug, "received data for time %ss is late from %sms", dataTime / (double)IClock::Rate, -delayInMs);
				}
			}
			getOutput(0)->emit(data);
		}

		std::shared_ptr<IClock> const clock;

		static auto const REGULATION_TOLERANCE_IN_MS = 300;
};
}

