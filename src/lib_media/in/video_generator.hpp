#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/picture.hpp"

namespace Modules {
namespace In {

class VideoGenerator : public ActiveModule {
	public:
		VideoGenerator(IModuleHost* host, int maxFrames = 0);
		bool work() override;

	private:
		IModuleHost* const m_host;
		uint64_t m_numFrames = 0;
		const int maxFrames;
		OutputPicture *output;
};

}
}
