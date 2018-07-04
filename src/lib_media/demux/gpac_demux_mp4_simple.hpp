#pragma once

#include "lib_modules/utils/helper.hpp"
#include <cstdint>

typedef struct __tag_isom GF_ISOFile;

namespace Modules {
namespace Demux {

class ISOFileReader;

class GPACDemuxMP4Simple : public ActiveModule {
	public:
		GPACDemuxMP4Simple(std::string const& path);
		~GPACDemuxMP4Simple();
		void work() override;

	private:
		std::unique_ptr<ISOFileReader> reader;
		OutputDefault* output;
};

}
}
