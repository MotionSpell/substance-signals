#pragma once

#include "lib_utils/log_sink.hpp" // Warning, Debug
#include "lib_utils/format.hpp"
#include "lib_modules/utils/helper.hpp"
#include "../common/attributes.hpp"
#include <thread>
#include <chrono>

namespace Modules {

class RegulatorMono : public ModuleS {
	public:
		RegulatorMono(KHost* host, std::shared_ptr<IClock> clock_)
			: m_host(host), clock(clock_) {
			m_output = addOutput();
		}

		void processOne(Data data) override {
			auto const timeTarget = data->get<PresentationTime>().time;
			auto const timeNow = fractionToClock(clock->now());
			auto const delayInMs = clockToTimescale(timeTarget - timeNow, 1000) - m_offsetInMs;
			if (delayInMs > 0) {
				if (delayInMs > FWD_TOLERANCE_IN_MS) {
					m_host->log(Warning, format("forward discontinuity detected (%s ms)", delayInMs).c_str());
					m_offsetInMs += delayInMs;
					return processOne(data);
				}

				if (delayInMs > REGULATION_TOLERANCE_IN_MS)
					m_host->log(Warning, format("will sleep for %s ms", delayInMs).c_str());
				std::this_thread::sleep_for(std::chrono::milliseconds(delayInMs));
			} else if (delayInMs < -REGULATION_TOLERANCE_IN_MS) {
				if (delayInMs < -BWD_TOLERANCE_IN_MS) {
					m_host->log(Warning, format("backward discontinuity detected (%s ms)", -delayInMs).c_str());
					m_offsetInMs += delayInMs;
					return processOne(data);
				}

				if (-delayInMs > std::abs(m_lastDelayInMs)) {
					char msg[256];
					sprintf(msg, "late data (%.2fs)", -delayInMs/1000.0);
					m_host->log(Warning, msg);
				}
			}
			m_lastDelayInMs = delayInMs;
			m_output->post(data);
		}

	private:
		KHost* const m_host;
		KOutput* m_output;
		int64_t m_lastDelayInMs = 0, m_offsetInMs = 0;

		std::shared_ptr<IClock> const clock;

		static auto const REGULATION_TOLERANCE_IN_MS = 300;
		static auto const FWD_TOLERANCE_IN_MS = 20000;
		static auto const BWD_TOLERANCE_IN_MS = 6000;
};
}

