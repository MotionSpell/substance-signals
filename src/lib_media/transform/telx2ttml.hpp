#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include <list>
#include <sstream>

namespace Modules {
namespace Transform {

struct Page {
	Page() {}
	const std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs, uint64_t idx) const;
	const std::string toSRT();

	uint64_t tsInMs=0, startTimeInMs=0, endTimeInMs=0, showTimestampInMs=0, hideTimestampInMs=0;
	uint32_t framesProduced = 0;
	std::stringstream ss;
};

class TeletextToTTML : public ModuleS {
public:
	enum TimingPolicy {
		AbsoluteUTC,     //USP
		RelativeToMedia, //14496-30
		RelativeToSplit  //MSS
	};

	TeletextToTTML(unsigned pageNum, const std::string &lang, uint64_t splitDurationIn180k, TimingPolicy timingPolicy);
	void process(Data data) override;

private:
	const std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs);
	void sendSample(const std::string &sample);
	void processTelx(DataAVPacket const * const pkt);
	void dispatch();

	OutputDataDefault<DataAVPacket> *output;
	const unsigned pageNum;
	std::string lang;
	const TimingPolicy timingPolicy;
	int64_t intClock = 0, extClock = 0;
	const uint64_t splitDurationIn180k;
	uint64_t delayIn180k = 2 * Clock::Rate, firstDataAbsTimeInMs = 0;
	std::list<std::unique_ptr<Page>> currentPages;
};

}
}
