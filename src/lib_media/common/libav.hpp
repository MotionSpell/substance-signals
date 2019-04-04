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

Metadata createMetadataPktLibavVideo(AVCodecContext* codecCtx);
Metadata createMetadataPktLibavAudio(AVCodecContext* codecCtx);
Metadata createMetadataPktLibavSubtitle(AVCodecContext* codecCtx);

struct AVPacketDeleter {
	void operator()(AVPacket *p);
};

class DataAVPacket : public DataBase, private IBuffer {
	public:
		DataAVPacket(size_t size = 0);
		~DataAVPacket();

		// DataBase
		const IBuffer* getBuffer() const override {
			return this;
		}

		IBuffer* getBuffer() override {
			return this;
		}

		// IBuffer
		Span data() override;
		SpanC data() const override;
		void resize(size_t size) override;

		AVPacket* getPacket() const;
		void restamp(int64_t offsetIn180k, uint64_t pktTimescale) const;

	private:
		std::unique_ptr<AVPacket, AVPacketDeleter> const pkt;
};

class PcmFormat;
class DataPcm;
void libavAudioCtxConvertLibav(const PcmFormat *cfg, int &sampleRate, enum AVSampleFormat &format, int &numChannels, uint64_t &layout);
void libavAudioCtxConvert(const PcmFormat *cfg, AVCodecContext *codecCtx);
void libavFrameDataConvert(const DataPcm *data, AVFrame *frame);
void libavFrame2pcmConvert(const AVFrame *frame, PcmFormat *cfg);

AVPixelFormat pixelFormat2libavPixFmt(PixelFormat format);
PixelFormat libavPixFmt2PixelFormat(AVPixelFormat avPixfmt);

void copyToPicture(AVFrame const* avFrame, DataPicture* pic);
extern "C" int avGetBuffer2(struct AVCodecContext *s, AVFrame *frame, int flags);

std::string avStrError(int err);

}

