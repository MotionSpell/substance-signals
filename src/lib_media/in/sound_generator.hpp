#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"

namespace Modules {
namespace In {

class SoundGenerator : public ActiveModule {
	public:
		SoundGenerator();
		bool work() override;

	private:
		double nextSample();
		uint64_t m_numSamples;
		PcmFormat pcmFormat;
		OutputPcm* output;
};

}
}
