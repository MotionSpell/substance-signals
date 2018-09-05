#pragma once

#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "../common/gpacpp.hpp"
#include <vector>

struct AVPacket;

extern "C" {
#include <gpac/esi.h> // GF_ESInterface
}

namespace gpacpp {
class M2TSMux;
}

namespace Modules {
namespace Mux {

typedef Queue<AVPacket*> DataInput;

class GPACMuxMPEG2TS : public ModuleDynI, public gpacpp::Init {
	public:
		GPACMuxMPEG2TS(IModuleHost* host, bool real_time, unsigned mux_rate, unsigned pcr_ms = 100, int64_t pcr_init_val = -1);
		~GPACMuxMPEG2TS();
		void process() override;

	private:
		IModuleHost* const m_host;

		void declareStream(Data data);
		GF_Err fillInput(GF_ESInterface *esi, u32 ctrl_type, size_t inputIdx);
		static GF_Err staticFillInput(GF_ESInterface *esi, u32 ctrl_type, void *param);

		std::unique_ptr<gpacpp::M2TSMux> muxer;
		std::vector<std::unique_ptr<DataInput>> inputData;
		OutputDefault *output;
};

}
}
