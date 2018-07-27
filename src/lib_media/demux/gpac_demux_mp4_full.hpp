#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Demux {

struct ISOProgressiveReader;

class GPACDemuxMP4Full : public ModuleS, private LogCap {
	public:
		GPACDemuxMP4Full();
		~GPACDemuxMP4Full();
		void process(Data data) override;

	private:
		bool openData();
		void updateData();
		bool processSample();
		bool safeProcessSample();

		std::unique_ptr<ISOProgressiveReader> reader;
		OutputDefault* output;
};

}
}
