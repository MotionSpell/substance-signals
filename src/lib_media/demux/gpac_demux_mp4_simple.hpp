#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Demux {

class ISOFileReader;

class GPACDemuxMP4Simple : public ActiveModule {
	public:
		GPACDemuxMP4Simple(std::string const& path);
		~GPACDemuxMP4Simple();
		bool work() override;

	private:
		std::unique_ptr<ISOFileReader> reader;
		OutputDefault* output;
};

}
}
