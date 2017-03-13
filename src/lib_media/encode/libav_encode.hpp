#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include "../common/picture.hpp"
#include "lib_utils/queue.hpp"
#include <string>

struct AVStream;

namespace ffpp {
class Frame;
}

namespace Modules {
namespace Encode {

struct LibavEncodeParams {
	enum VideoCodecType {
		Software,
		Hardware_qsv,
		Hardware_nvenc
	};

	//video only
	Resolution res = VIDEO_RESOLUTION;
	int bitrate_v = 300000;
	int GOPSize = 25;
	int frameRateNum = 25;
	int frameRateDen = 1;
	bool isLowLatency = false;
	VideoCodecType codecType = Software;
	PixelFormat pixelFormat = UNKNOWN_PF; //set by the encoder

	//audio only
	int bitrate_a = 128000;
	int sampleRate = 44100;
	int numChannels = 2;

	std::string avcodecCustom;
};

class LibavEncode : public ModuleS {
	public:
		enum Type {
			Video,
			Audio,
			Unknown
		};

		LibavEncode(Type type, LibavEncodeParams &params = *uptr(new LibavEncodeParams));
		~LibavEncode();
		void process(Data data) override;
		void flush() override;

	private:
		bool processAudio(const DataPcm *data);
		bool processVideo(const DataPicture *data);

		AVCodecContext *codecCtx;
		std::unique_ptr<PcmFormat> pcmFormat = nullptr;
		std::unique_ptr<ffpp::Frame> const avFrame;
		OutputDataDefault<DataAVPacket>* output;
};

}
}
