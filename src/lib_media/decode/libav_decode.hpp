#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include "../common/pcm.hpp"
#include <map>

struct AVCodecContext;

namespace ffpp {
class Frame;
}

class AudioConverter;

namespace Modules {
namespace Decode {

class LibavDecode : public ModuleS, public LibavDirectRendering {
	public:
		LibavDecode(const MetadataPktLibav &metadata);
		~LibavDecode();
		void process(Data data) override;
		void flush() override;

	private:
		bool processAudio(const DataAVPacket*);
		bool processVideo(const DataAVPacket*);
		LibavDirectRenderingContext* getPicture(const Resolution &res, const Resolution &resInternal, const PixelFormat &format) override;

		AVCodecContext * const codecCtx;
		std::unique_ptr<ffpp::Frame> const avFrame;
		OutputPicture* videoOutput;
		OutputPcm* audioOutput;
};

}
}
