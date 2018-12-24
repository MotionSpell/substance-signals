#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/picture.hpp"

namespace Modules {
namespace In {

class VideoGenerator : public Module {
	public:
		VideoGenerator(KHost* host, int maxFrames = 0);
		void process() override;

	private:
		KHost* const m_host;
		uint64_t m_numFrames = 0;
		const int maxFrames;
		OutputPicture *output;
};

}
}
