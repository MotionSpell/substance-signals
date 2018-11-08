#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"

namespace Modules {
namespace In {

class SoundGenerator : public ActiveModule {
	public:
		SoundGenerator(KHost* host);
		bool work() override;

	private:
		KHost* const m_host;
		double nextSample();
		uint64_t m_numSamples;
		PcmFormat pcmFormat;
		OutputPcm* output;
};

}
}
