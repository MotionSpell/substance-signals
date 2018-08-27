#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"

namespace Modules {
namespace In {

class SoundGenerator : public ActiveModule {
	public:
		SoundGenerator(IModuleHost* host);
		bool work() override;

	private:
		IModuleHost* const m_host;
		double nextSample();
		uint64_t m_numSamples;
		PcmFormat pcmFormat;
		OutputPcm* output;
};

}
}
