#pragma once

#include "lib_modules/core/module.hpp"

struct AVFormatContext;
struct AVPacket;

namespace Modules {
namespace Mux {

class LibavMux : public ModuleDynI {
	public:
		LibavMux(const std::string &baseName, const std::string &fmt);
		~LibavMux();
		void process() override;

	private:
		void ensureHeader();
		AVPacket * getFormattedPkt(Data data);
		void declareStream(Data stream);

		struct AVFormatContext *m_formatCtx;
		bool m_headerWritten = false;
		bool m_inbandMetadata = false;
};

}
}
