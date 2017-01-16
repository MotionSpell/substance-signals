#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_modules/core/clock.hpp"
#include "../common/libav.hpp"
#include <sstream>

namespace Modules {
namespace Transform {

struct Page;

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
	void sendSample(const std::string &sample);
	void generateSamplesUntilTime(uint64_t time, Page const * const page);
	OutputDataDefault<DataAVPacket> *output;
	const unsigned pageNum;
	const TimingPolicy timingPolicy;
	const uint64_t splitDurationIn180k;
	uint64_t intClock = 0, extClock = 0, delayIn180k = 2 * Clock::Rate;
};

}
}
