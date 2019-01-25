#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"

#include <limits> // numeric_limits

namespace Modules {
namespace Transform {

class AudioGapFiller : public ModuleS {
	public:
		AudioGapFiller(KHost* host, uint64_t toleranceInFrames = 10);
		void processOne(Data data) override;

	private:
		KHost* const m_host;
		uint64_t toleranceInFrames, accumulatedTimeInSR = std::numeric_limits<uint64_t>::max();
		KOutput *output;
};

}
}
