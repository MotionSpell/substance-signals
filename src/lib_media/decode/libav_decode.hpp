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

class LibavDecode : public ModuleS, private PictureAllocator {
	public:
		LibavDecode(std::shared_ptr<const MetadataPktLibav> metadata);
		~LibavDecode();
		void process(Data data) override;
		void flush() override;

	private:
		void processPacket(AVPacket const * pkt);
		void processAudio();
		void processVideo();
		void setMediaTime(DataBase* data);
		PictureAllocator::PictureContext* getPicture(Resolution res, Resolution resInternal, PixelFormat format) override;

		std::shared_ptr<AVCodecContext> const codecCtx;
		std::unique_ptr<ffpp::Frame> const avFrame;
		OutputPicture *videoOutput;
		OutputPcm *audioOutput;
		std::function<void(void)> deliverOutput;
};

}
}
