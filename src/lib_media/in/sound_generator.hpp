#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"

namespace Modules {
namespace In {

class SoundGenerator : public Module {
	public:
		SoundGenerator(KHost* host);
		void process() override;

	private:
		KHost* const m_host;
		double nextSample();
		uint64_t m_numSamples;
		OutputDefault* output;
};

}
}
