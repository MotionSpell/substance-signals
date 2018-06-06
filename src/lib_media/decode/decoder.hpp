#pragma once

#include "../common/metadata.hpp"
#include "../common/picture_allocator.hpp"
#include "../common/pcm.hpp"

struct AVCodecContext;
struct AVPacket;

namespace ffpp {
class Frame;
}

namespace Modules {
namespace Decode {

class Decoder : public ModuleS, private PictureAllocator {
	public:
		Decoder(StreamType type);
		~Decoder();
		void process(Data data) override;
		void flush() override;

	private:
		void openDecoder(const MetadataPkt* metadata);
		void processPacket(AVPacket const * pkt);
		std::shared_ptr<DataBase> processAudio();
		std::shared_ptr<DataBase> processVideo();
		void setMediaTime(DataBase* data);
		PictureAllocator::PictureContext* getPicture(Resolution res, Resolution resInternal, PixelFormat format) override;

		std::shared_ptr<AVCodecContext> codecCtx;
		std::unique_ptr<ffpp::Frame> const avFrame;
		OutputPicture *videoOutput;
		OutputPcm *audioOutput;
		std::function<std::shared_ptr<DataBase>(void)> getDecompressedData;
};

}
}
