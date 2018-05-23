#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/libav.hpp"

namespace Modules {
namespace In {

class VideoGenerator : public ModuleS {
	public:
		VideoGenerator();
		void process(Data data) override;

	private:
		uint64_t m_numFrames = 0;
		OutputPicture *output;
};

}
}
