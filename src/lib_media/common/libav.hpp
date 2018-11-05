#pragma once

#include "picture.hpp"
#include "pcm.hpp"
#include "metadata.hpp"
#include <memory>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
}

std::shared_ptr<AVCodecContext> shptr(AVCodecContext *p);

namespace Modules {

struct MetadataPktLibav : MetadataPkt {
	MetadataPktLibav(std::shared_ptr<AVCodecContext> codecCtx);
};

struct MetadataPktLibavVideo : MetadataPktLibav {
	MetadataPktLibavVideo(std::shared_ptr<AVCodecContext> codecCtx);
	PixelFormat pixelFormat;
	Fraction sampleAspectRatio;
	Resolution resolution;
	Fraction framerate;
};

struct MetadataPktLibavAudio : MetadataPktLibav {
	MetadataPktLibavAudio(std::shared_ptr<AVCodecContext> codecCtx);
	uint32_t numChannels;
	uint32_t sampleRate;
	uint8_t bitsPerSample;
	uint32_t frameSize;
	bool planar;
	AudioSampleFormat format;
	AudioLayout layout;
};

struct MetadataPktLibavSubtitle : MetadataPktLibav {
	MetadataPktLibavSubtitle(std::shared_ptr<AVCodecContext>codecCtx) : MetadataPktLibav(codecCtx) {}
};

struct AVPacketDeleter {
	void operator()(AVPacket *p);
};

class DataAVPacket : public DataBase {
	public:
		DataAVPacket(size_t size = 0);
		~DataAVPacket();
		bool isRecyclable() const override {
			return false;
		}
		Span data() override;
		SpanC data() const override;
		void resize(size_t size) override;

		AVPacket* getPacket() const;
		void restamp(int64_t offsetIn180k, uint64_t pktTimescale) const;
		bool isRap() const;

	private:
		std::unique_ptr<AVPacket, AVPacketDeleter> const pkt;
};

class PcmFormat;
class DataPcm;
void libavAudioCtxConvertLibav(const PcmFormat *cfg, int &sampleRate, enum AVSampleFormat &format, int &numChannels, uint64_t &layout);
void libavAudioCtxConvert(const PcmFormat *cfg, AVCodecContext *codecCtx);
void libavFrameDataConvert(const DataPcm *data, AVFrame *frame);
void libavFrame2pcmConvert(const AVFrame *frame, PcmFormat *cfg);

PcmFormat toPcmFormat(std::shared_ptr<const MetadataPktLibavAudio> meta);

AVPixelFormat pixelFormat2libavPixFmt(PixelFormat format);
PixelFormat libavPixFmt2PixelFormat(AVPixelFormat avPixfmt);

void copyToPicture(AVFrame const* avFrame, DataPicture* pic);
extern "C" int avGetBuffer2(struct AVCodecContext *s, AVFrame *frame, int flags);

std::string avStrError(int err);

}

