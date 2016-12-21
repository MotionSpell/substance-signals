#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include <sstream>

struct Page;

namespace Modules {
namespace Transform {

class TeletextToTTML : public ModuleS {
public:
	TeletextToTTML(unsigned page, uint64_t splitDurationIn180k);
	void process(Data data) override;

private:
	void sendSample(std::stringstream *page);
	void generateEmptySamplesUntilTime(uint64_t time);
	std::string writeTTML(std::stringstream *page);
	OutputDataDefault<DataAVPacket> *output;
	unsigned page;
	uint64_t intClock = 0, extClock = 0, splitDurationIn180k, delayIn180k = 2 * Clock::Rate;
};

}
}
