#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Demux {

struct ISOProgressiveReader;

class GPACDemuxMP4Full : public ModuleS {
	public:
		GPACDemuxMP4Full();
		~GPACDemuxMP4Full();
		void process(Data data) override;

	private:
		bool openData();
		bool updateData();
		bool processSample();
		void processData();
		bool safeProcessSample();

		std::unique_ptr<ISOProgressiveReader> reader;
		OutputDefault* output;
};

}
}
