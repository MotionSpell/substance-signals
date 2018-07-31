#pragma once

#include "lib_modules/core/log.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_ffpp/ffpp.hpp"

struct AVFormatContext;
struct AVPacket;

struct MuxConfig {
	std::string baseName;
	std::string format;
	std::string options;
};

namespace Modules {
namespace Mux {

class LibavMux : public ModuleDynI, private LogCap {
	public:
		LibavMux(MuxConfig cfg);
		~LibavMux();
		void process() override;

	private:
		static void formatsList();
		void ensureHeader();
		AVPacket * getFormattedPkt(Data data);
		bool declareStream(Data stream, size_t inputIdx);

		struct AVFormatContext *m_formatCtx;
		std::map<size_t, size_t> inputIdx2AvStream;
		ffpp::Dict optionsDict;
		bool m_headerWritten = false;
		bool m_inbandMetadata = false;
};

}
}
