#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/libav.hpp"
#include "../common/pcm.hpp"

struct AVCodecContext;

namespace ffpp {
class Frame;
}

namespace Modules {
namespace Decode {

class LibavDecode : public ModuleS, public LibavDirectRendering {
	public:
		LibavDecode(std::shared_ptr<const MetadataPktLibav> metadata);
		~LibavDecode();
		void process(Data data) override;
		void flush() override;

	private:
		void processPacket(AVPacket const * pkt);
		bool processAudio(AVPacket const * const pkt);
		bool processVideo(AVPacket const * const pkt);
		LibavDirectRenderingContext* getPicture(const Resolution &res, const Resolution &resInternal, const PixelFormat &format) override;

		std::shared_ptr<AVCodecContext> const codecCtx;
		std::unique_ptr<ffpp::Frame> const avFrame;
		OutputPicture *videoOutput;
		OutputPcm *audioOutput;
};

}
}
