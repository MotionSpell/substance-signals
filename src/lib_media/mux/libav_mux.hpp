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
		void ensureHeader();
		AVPacket * getFormattedPkt(Data data);
		void declareStream(Data stream);

		struct AVFormatContext *m_formatCtx;
		std::unique_ptr<ffpp::AvIO> m_avio = nullptr;
		ffpp::Dict optionsDict;
		bool m_headerWritten = false;
		bool m_inbandMetadata = false;
};

}
}
