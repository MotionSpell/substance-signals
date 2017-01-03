#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include <sstream>

namespace Modules {
namespace Transform {

struct Page;

class TeletextToTTML : public ModuleS {
public:
	TeletextToTTML(unsigned page, uint64_t splitDurationIn180k);
	void process(Data data) override;

private:
	void sendSample(const std::string &sample);
	void generateSamplesUntilTime(uint64_t time, Page const * const page);
	OutputDataDefault<DataAVPacket> *output;
	unsigned page;
	uint64_t intClock = 0, extClock = 0, splitDurationIn180k, delayIn180k = 2 * Clock::Rate;
};

}
}
