#pragma once

#include "picture.hpp"
#include "lib_modules/core/metadata.hpp"
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

class MetadataPktLibav : public MetadataPkt {
	public:
		MetadataPktLibav(std::shared_ptr<AVCodecContext> codecCtx, int id = -1);
		virtual ~MetadataPktLibav() {}
		StreamType getStreamType() const override;
		int64_t getBitrate() const;
		Fraction getTimeScale() const;
		std::string getCodecName() const;
		int getId() const;

		// deprecated, as it fails to isolate the caller from ffmpeg
		std::shared_ptr<AVCodecContext> getAVCodecContext() const;

	protected:
		std::shared_ptr<AVCodecContext> codecCtx;
		int id; /*format specific id e.g. PID for MPEG2-TS. -1 is uninitialized*/
};

class MetadataPktLibavVideo : public MetadataPktLibav {
	public:
		MetadataPktLibavVideo(std::shared_ptr<AVCodecContext> codecCtx, int id = -1) : MetadataPktLibav(codecCtx, id) {}
		PixelFormat getPixelFormat() const;
		Resolution getResolution() const;
		Fraction getFrameRate() const;
		void getExtradata(const uint8_t *&extradata, size_t &extradataSize) const;
};

class MetadataPktLibavAudio : public MetadataPktLibav {
	public:
		MetadataPktLibavAudio(std::shared_ptr<AVCodecContext> codecCtx, int id = -1) : MetadataPktLibav(codecCtx, id) {}
		std::string getCodecName() const;
		uint32_t getNumChannels() const;
		uint32_t getSampleRate() const;
		uint8_t getBitsPerSample() const;
		uint32_t getFrameSize() const;
		void getExtradata(const uint8_t *&extradata, size_t &extradataSize) const;
};

class MetadataPktLibavSubtitle : public MetadataPktLibav {
	public:
		MetadataPktLibavSubtitle(std::shared_ptr<AVCodecContext>codecCtx, int id = -1) : MetadataPktLibav(codecCtx, id) {}
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
		uint8_t* data() override;
		uint8_t const* data() const override;
		uint64_t size() const override;
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
void libavAudioCtx2pcmConvert(std::shared_ptr<const MetadataPktLibavAudio> meta, PcmFormat *cfg);
void libavFrameDataConvert(const DataPcm *data, AVFrame *frame);
void libavFrame2pcmConvert(const AVFrame *frame, PcmFormat *cfg);

AVPixelFormat pixelFormat2libavPixFmt(PixelFormat format);
PixelFormat libavPixFmt2PixelFormat(AVPixelFormat avPixfmt);

void copyToPicture(AVFrame const* avFrame, DataPicture* pic);
extern "C" int avGetBuffer2(struct AVCodecContext *s, AVFrame *frame, int flags);

std::string avStrError(int err);

}

