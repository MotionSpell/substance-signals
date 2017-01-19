#pragma once

#include "picture.hpp"
#include "lib_modules/core/output.hpp"
#include "lib_modules/core/metadata.hpp"
#include <cstdarg>
#include <memory>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
#ifdef _MSC_VER
enum AVPixelFormat;
enum AVSampleFormat;
#undef PixelFormat
#else
extern "C" {
#include <libavcodec/avcodec.h>
#undef PixelFormat
}
#endif

namespace Modules {

class MetadataPktLibav : public IMetadata {
	public:
		//Doesn't take codecCtx ownership
		MetadataPktLibav(AVCodecContext *codecCtx, int id = -1);
		virtual ~MetadataPktLibav() {}
		StreamType getStreamType() const override;
		int64_t getBitrate() const;
		AVCodecContext* getAVCodecContext() const;
		int getId() const;

	protected:
		AVCodecContext *codecCtx;
		int id; /*format specific id e.g. PID for MPEG2-TS. -1 is uninitialized*/
};

class MetadataPktLibavVideo : public MetadataPktLibav {
	public:
		MetadataPktLibavVideo(AVCodecContext *codecCtx, int id = -1) : MetadataPktLibav(codecCtx, id) {}
		PixelFormat getPixelFormat() const;
		Resolution getResolution() const;
		uint32_t getTimeScaleNum() const;
		uint32_t getTimeScaleDen() const;
		void getExtradata(const uint8_t *&extradata, size_t &extradataSize) const;
};

class MetadataPktLibavAudio : public MetadataPktLibav {
	public:
		MetadataPktLibavAudio(AVCodecContext *codecCtx, int id = -1) : MetadataPktLibav(codecCtx, id) {}
		std::string getCodecName() const;
		uint32_t getNumChannels() const;
		uint32_t getSampleRate() const;
		uint8_t getBitsPerSample() const;
		uint32_t getFrameSize() const;
		void getExtradata(const uint8_t *&extradata, size_t &extradataSize) const;
};

class MetadataPktLibavSubtitle : public MetadataPktLibav {
public:
	MetadataPktLibavSubtitle(AVCodecContext *codecCtx, int id = -1) : MetadataPktLibav(codecCtx, id) {}
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

	private:
		std::unique_ptr<AVPacket, AVPacketDeleter> const pkt;
};

class PcmFormat;
class DataPcm;
void libavAudioCtxConvertLibav(const PcmFormat *cfg, int &sampleRate, enum AVSampleFormat &format, int &numChannels, uint64_t &layout);
void libavAudioCtxConvert(const PcmFormat *cfg, AVCodecContext *codecCtx);
void libavAudioCtx2pcmConvert(const AVCodecContext *codecCtx, PcmFormat *cfg);
void libavFrameDataConvert(const DataPcm *data, AVFrame *frame);
void libavFrame2pcmConvert(const AVFrame *frame, PcmFormat *cfg);

void pixelFormat2libavPixFmt(const enum PixelFormat format, enum AVPixelFormat &avPixfmt);
enum PixelFormat libavPixFmt2PixelFormat(const enum AVPixelFormat &avPixfmt);

struct LibavDirectRendering {
	virtual DataPicture* getPicture(const Resolution &res, const PixelFormat &format) = 0;
};
void copyToPicture(AVFrame const* avFrame, DataPicture* pic);
extern "C" int avGetBuffer2(struct AVCodecContext *s, AVFrame *frame, int flags);
void avLog(void *avcl, int level, const char *fmt, va_list vl);

}
