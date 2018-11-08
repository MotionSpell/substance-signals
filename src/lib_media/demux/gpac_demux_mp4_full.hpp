#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Demux {

struct ISOProgressiveReader;

class GPACDemuxMP4Full : public ModuleS {
	public:
		GPACDemuxMP4Full(KHost* host);
		~GPACDemuxMP4Full();
		void process(Data data) override;

	private:
		bool openData();
		void updateData();
		bool processSample();
		bool safeProcessSample();

		KHost* const m_host;

		std::unique_ptr<ISOProgressiveReader> reader;
		OutputDefault* output;
};

}
}
