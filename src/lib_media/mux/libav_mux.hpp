#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_ffpp/ffpp.hpp"

struct AVFormatContext;
struct AVPacket;

namespace Modules {
namespace Mux {

class LibavMux : public ModuleDynI {
	public:
		LibavMux(const std::string &baseName, const std::string &format, const std::string &options = "");
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
