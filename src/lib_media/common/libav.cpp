#include "libav.hpp"
#include "pcm.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/tools.hpp"
#include <cassert>
#include <cstdio>
#include <mutex>
#include <string.h>

extern "C" {
#include <libavcodec/avcodec.h>
#undef PixelFormat
}

namespace {

int av_lockmgr(void **mutex, enum AVLockOp op) {
	if (NULL == mutex) {
		return -1;
	}

	switch (op) {
	case AV_LOCK_CREATE: {
		std::mutex *m = new std::mutex();
		*mutex = static_cast<void*>(m);
		break;
	}
	case AV_LOCK_OBTAIN: {
		std::mutex *m = static_cast<std::mutex*>(*mutex);
		m->lock();
		break;
	}
	case AV_LOCK_RELEASE: {
		std::mutex *m = static_cast<std::mutex*>(*mutex);
		m->unlock();
		break;
	}
	case AV_LOCK_DESTROY: {
		std::mutex *m = static_cast<std::mutex*>(*mutex);
		delete m;
		break;
	}
	default:
		assert(0);
		break;
	}
	return 0;
}
void av_lockmgr_register() {
	av_lockmgr_register(&av_lockmgr);
}
auto g_InitAvLockMgr = runAtStartup(&av_lockmgr_register);

Level avLogLevel(int level) {
	switch (level) {
	case AV_LOG_QUIET:
	case AV_LOG_PANIC:
	case AV_LOG_FATAL:
	case AV_LOG_ERROR:
		return Warning;
	case AV_LOG_WARNING:
		return Info;
	case AV_LOG_INFO:
	case AV_LOG_VERBOSE:
		return Debug;
	case AV_LOG_DEBUG:
	case AV_LOG_TRACE:
		return Quiet;
	default:
		assert(0);
		return Debug;
	}
}

const char* avlogLevelName(int level) {
	switch (level) {
	case AV_LOG_QUIET:
		return "quiet";
	case AV_LOG_PANIC:
		return "panic";
	case AV_LOG_FATAL:
		return "fatal";
	case AV_LOG_ERROR:
		return "error";
	case AV_LOG_WARNING:
		return "warning";
	case AV_LOG_INFO:
		return "info";
	case AV_LOG_VERBOSE:
		return "verbose";
	case AV_LOG_DEBUG:
		return "debug";
	case AV_LOG_TRACE:
		return "trace";
	default:
		assert(0);
		return "unknown";
	}
}
}

namespace Modules {

//MetadataPktLibav
MetadataPktLibav::MetadataPktLibav(AVCodecContext *codecCtx, int id)
	: codecCtx(codecCtx), id(id) {
}

StreamType MetadataPktLibav::getStreamType() const {
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: return VIDEO_PKT;
	case AVMEDIA_TYPE_AUDIO: return AUDIO_PKT;
	default: throw std::runtime_error("Unknown stream type. Only audio and video handled.");
	}
}

AVCodecContext* MetadataPktLibav::getAVCodecContext() const {
	return codecCtx;
}

int MetadataPktLibav::getId() const {
	return id;
}

int64_t MetadataPktLibav::getBitrate() const {
	return codecCtx->bit_rate;
}

PixelFormat MetadataPktLibavVideo::getPixelFormat() const {
	return libavPixFmt2PixelFormat(codecCtx->pix_fmt);
}

Resolution MetadataPktLibavVideo::getResolution() const {
	return Resolution(codecCtx->width, codecCtx->height);
}

uint32_t MetadataPktLibavVideo::getTimeScaleNum() const {
	if (codecCtx->time_base.num == 0)
		throw std::runtime_error("Unsupported video time scale numerator.");
	return codecCtx->ticks_per_frame * codecCtx->time_base.num;
}

uint32_t MetadataPktLibavVideo::getTimeScaleDen() const {
	if (codecCtx->time_base.den == 0)
		throw std::runtime_error("Unsupported video time scale denumerator.");
	return codecCtx->time_base.den;
}

void MetadataPktLibavVideo::getExtradata(const uint8_t *&extradata, size_t &extradataSize) const {
	extradata = codecCtx->extradata;
	extradataSize = codecCtx->extradata_size;
}

//MetadataPktLibavAudio
std::string MetadataPktLibavAudio::getCodecName() const {
	return avcodec_get_name(codecCtx->codec_id);
}

uint32_t MetadataPktLibavAudio::getNumChannels() const {
	return codecCtx->channels;
}

uint32_t MetadataPktLibavAudio::getSampleRate() const {
	return codecCtx->sample_rate;
}

uint8_t MetadataPktLibavAudio::getBitsPerSample() const {
	return av_get_bytes_per_sample(codecCtx->sample_fmt) * 8;
}

uint32_t MetadataPktLibavAudio::getFrameSize() const {
	return codecCtx->frame_size;
}

void MetadataPktLibavAudio::getExtradata(const uint8_t *&extradata, size_t &extradataSize) const {
	extradata = codecCtx->extradata;
	extradataSize = codecCtx->extradata_size;
}

//conversions
void libavAudioCtxConvertLibav(const Modules::PcmFormat *cfg, int &sampleRate, AVSampleFormat &format, int &numChannels, uint64_t &layout) {
	sampleRate = cfg->sampleRate;

	switch (cfg->layout) {
	case Modules::Mono: layout = AV_CH_LAYOUT_MONO; break;
	case Modules::Stereo: layout = AV_CH_LAYOUT_STEREO; break;
	default: throw std::runtime_error("Unknown libav audio layout");
	}
	numChannels = av_get_channel_layout_nb_channels(layout);
	assert(numChannels == cfg->numChannels);

	switch (cfg->sampleFormat) {
	case Modules::S16: format = av_get_alt_sample_fmt(AV_SAMPLE_FMT_S16, cfg->numPlanes > 1); break;
	case Modules::F32: format = av_get_alt_sample_fmt(AV_SAMPLE_FMT_FLT, cfg->numPlanes > 1); break;
	default: throw std::runtime_error("Unknown libav audio format (1)");
	}
}

void libavAudioCtxConvert(const PcmFormat *cfg, AVCodecContext *codecCtx) {
	libavAudioCtxConvertLibav(cfg, codecCtx->sample_rate, codecCtx->sample_fmt, codecCtx->channels, codecCtx->channel_layout);
}

void libavAudioCtx2pcmConvert(const AVCodecContext *codecCtx, PcmFormat *cfg) {
	cfg->sampleRate = codecCtx->sample_rate;

	cfg->numChannels = cfg->numPlanes = codecCtx->channels;
	switch (codecCtx->sample_fmt) {
	case AV_SAMPLE_FMT_S16: cfg->numPlanes = 1;
	case AV_SAMPLE_FMT_S16P:
		cfg->sampleFormat = Modules::S16;
		break;
	case AV_SAMPLE_FMT_FLT: cfg->numPlanes = 1;
	case AV_SAMPLE_FMT_FLTP:
		cfg->sampleFormat = Modules::F32;
		break;
	default:
		throw std::runtime_error("Unknown libav audio format (2)");
	}

	switch (codecCtx->channel_layout) {
	case AV_CH_LAYOUT_MONO:   cfg->layout = Modules::Mono; break;
	case AV_CH_LAYOUT_STEREO: cfg->layout = Modules::Stereo; break;
	default:
		switch (cfg->numChannels) {
		case 1: cfg->layout = Modules::Mono; break;
		case 2: cfg->layout = Modules::Stereo; break;
		default: throw std::runtime_error("Unknown libav audio layout");
		}
	}
}

void libavFrame2pcmConvert(const AVFrame *frame, PcmFormat *cfg) {
	cfg->sampleRate = frame->sample_rate;

	cfg->numChannels = cfg->numPlanes = frame->channels;
	switch (frame->format) {
	case AV_SAMPLE_FMT_S16: cfg->numPlanes = 1;
	case AV_SAMPLE_FMT_S16P:
		cfg->sampleFormat = Modules::S16;
		break;
	case AV_SAMPLE_FMT_FLT: cfg->numPlanes = 1;
	case AV_SAMPLE_FMT_FLTP:
		cfg->sampleFormat = Modules::F32;
		break;
	default:
		throw std::runtime_error("Unknown libav audio format (3)");
	}

	switch (frame->channel_layout) {
	case AV_CH_LAYOUT_MONO:   cfg->layout = Modules::Mono; break;
	case AV_CH_LAYOUT_STEREO: cfg->layout = Modules::Stereo; break;
	default:
		switch (cfg->numChannels) {
		case 1: cfg->layout = Modules::Mono; break;
		case 2: cfg->layout = Modules::Stereo; break;
		default: throw std::runtime_error("Unknown libav audio layout");
		}
	}
}

void libavFrameDataConvert(const DataPcm *pcmData, AVFrame *frame) {
	auto const& format = pcmData->getFormat();
	AVSampleFormat avsf; libavAudioCtxConvertLibav(&format, frame->sample_rate, avsf, frame->channels, frame->channel_layout); frame->format = (int)avsf;
	for (size_t i = 0; i < format.numPlanes; ++i) {
		frame->data[i] = pcmData->getPlane(i);
		if (i == 0)
			frame->linesize[i] = (int)pcmData->getPlaneSize(i) / format.numChannels;
		else
			frame->linesize[i] = 0;
	}
	frame->nb_samples = (int)(pcmData->size() / format.getBytesPerSample());
}

void pixelFormat2libavPixFmt(const enum PixelFormat format, AVPixelFormat &avPixfmt) {
	switch (format) {
	case YUV420P: avPixfmt = AV_PIX_FMT_YUV420P; break;
	case YUYV422: avPixfmt = AV_PIX_FMT_YUYV422; break;
	case NV12: avPixfmt = AV_PIX_FMT_NV12; break;
	case RGB24: avPixfmt = AV_PIX_FMT_RGB24; break;
	default: throw std::runtime_error("Unknown pixel format to convert (1). Please contact your vendor.");
	}
}

enum PixelFormat libavPixFmt2PixelFormat(const AVPixelFormat &avPixfmt) {
	switch (avPixfmt) {
	case AV_PIX_FMT_YUV420P: case AV_PIX_FMT_YUVJ420P: return YUV420P;
	case AV_PIX_FMT_YUYV422: return YUYV422;
	case AV_PIX_FMT_NV12: return NV12;
	case AV_PIX_FMT_RGB24: return RGB24;
	default: throw std::runtime_error("Unknown pixel format to convert (2). Please contact your vendor.");
	}
}

//DataAVPacket
void AVPacketDeleter::operator()(AVPacket *p) {
	av_free_packet(p);
	delete(p);
}

DataAVPacket::DataAVPacket(size_t size)
	: pkt(std::unique_ptr<AVPacket, AVPacketDeleter>(new AVPacket)) {
	av_init_packet(pkt.get());
	av_free_packet(pkt.get());
	if (size)
		av_new_packet(pkt.get(), (int)size);
}

DataAVPacket::~DataAVPacket() {
	Log::msg(Debug, "Freeing %s, pts=%s", this, pkt->pts);
}

uint8_t* DataAVPacket::data() {
	return pkt->data;
}

uint8_t const* DataAVPacket::data() const {
	return pkt->data;
}

uint64_t DataAVPacket::size() const {
	return pkt->size;
}

AVPacket* DataAVPacket::getPacket() const {
	return pkt.get();
}

void DataAVPacket::restamp(int64_t offsetIn180k, uint64_t pktTimescale) const {
	auto p = pkt.get();
	auto const offset = clockToTimescale(offsetIn180k, pktTimescale);
	p->pts += offset;
	p->dts += offset;
}

void DataAVPacket::resize(size_t /*size*/) {
	assert(0);
}

void avLog(void* /*avcl*/, int level, const char *fmt, va_list vl) {
#if defined(__CYGWIN__) // cygwin does not have vsnprintf in std=c++11 mode. To be removed when cygwin is fixed
	Log::msg(avLogLevel(level), "[libav-log::%s] %s", avlogLevelName(level), fmt);
#else
	char buffer[1024];
	std::vsnprintf(buffer, sizeof(buffer)-1, fmt, vl);

	// remove trailing end of line
	{
		auto const N = strlen(buffer);
		if(N > 0 && buffer[N-1] == '\n')
			buffer[N-1] = 0;
	}
	Log::msg(avLogLevel(level), "[libav-log::%s] %s", avlogLevelName(level), buffer);
#endif
}

}

