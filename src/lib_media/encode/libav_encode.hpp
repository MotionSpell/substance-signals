#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"
#include "../common/picture.hpp"
#include "lib_utils/queue.hpp"
#include <string>

struct AVCodecContext;
struct AVStream;
struct AVFrame;

namespace ffpp {
class Frame;
}

namespace Modules {
class DataAVPacket;

namespace Encode {

class LibavEncode : public ModuleS {
	public:
		enum Type {
			Video,
			Audio,
			Unknown
		};

		struct Params {
			Params() {}

			//video only
			Resolution res = VIDEO_RESOLUTION;
			int bitrate_v = 300000;
			Fraction GOPSize = Fraction(25, 1);
			Fraction frameRate = Fraction(25, 1);
			bool isLowLatency = false;
			VideoCodecType codecType = Software;
			PixelFormat pixelFormat = UNKNOWN_PF; //set by the encoder

			//audio only
			int bitrate_a = 128000;
			int sampleRate = 44100;
			int numChannels = 2;

			std::string avcodecCustom;
		};

		LibavEncode(Type type, Params &params = *make_unique<Params>());
		~LibavEncode();
		void process(Data data) override;
		void flush() override;

	private:
		void encodeFrame(AVFrame* frame);
		void computeFrameAttributes(AVFrame * const f, const int64_t currMediaTime);

		std::shared_ptr<AVCodecContext> codecCtx;
		std::unique_ptr<PcmFormat> pcmFormat = nullptr;
		std::unique_ptr<ffpp::Frame> const avFrame;
		OutputDataDefault<DataAVPacket>* output;
		int64_t firstMediaTime, prevMediaTime;
		Fraction GOPSize;
};

}
}
