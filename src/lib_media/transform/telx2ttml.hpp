#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_modules/core/clock.hpp"
#include "../common/libav.hpp"
#include <list>
#include <sstream>

namespace Modules {
namespace Transform {

struct Page {
	Page() {}
	const std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs, uint64_t idx) const;
	const std::string toSRT();

	uint64_t tsInMs, startTimeInMs, endTimeInMs, show_timestamp, hide_timestamp;
	uint32_t frames_produced = 0;
	std::stringstream ss;
};

class TeletextToTTML : public ModuleS {
public:
	enum TimingPolicy {
		AbsoluteUTC,     //USP
		RelativeToMedia, //14496-30
		RelativeToSplit  //MSS
	};

	TeletextToTTML(unsigned pageNum, uint64_t splitDurationIn180k, TimingPolicy timingPolicy);
	void process(Data data) override;

private:
	const std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs);
	void sendSample(const std::string &sample);
	OutputDataDefault<DataAVPacket> *output;
	const unsigned pageNum;
	const TimingPolicy timingPolicy;
	const uint64_t splitDurationIn180k;
	uint64_t intClock = 0, extClock = 0, delayIn180k = 2 * Clock::Rate;
	std::list<std::unique_ptr<Page>> currentPages;
};

}
}
