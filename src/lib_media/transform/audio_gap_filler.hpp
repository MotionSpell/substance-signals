#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/pcm.hpp"

namespace Modules {
namespace Transform {

class AudioGapFiller : public ModuleS {
	public:
		AudioGapFiller(uint64_t toleranceInFrames = 10);
		void process(Data data) override;

	private:
		uint64_t toleranceInFrames, accumulatedTimeInSR = std::numeric_limits<uint64_t>::max();
		OutputPcm *output;
};

}
}
