#pragma once

#include <string>

struct Mp4DemuxConfig {
	std::string path;
};

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Demux {

class ISOFileReader;

class GPACDemuxMP4Simple : public ActiveModule {
	public:
		GPACDemuxMP4Simple(IModuleHost* host, Mp4DemuxConfig const* cfg);
		~GPACDemuxMP4Simple();
		bool work() override;

	private:
		IModuleHost* const m_host;
		std::unique_ptr<ISOFileReader> reader;
		OutputDefault* output;
};

}
}
