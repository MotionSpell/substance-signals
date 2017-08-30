#include "libav.hpp"
#include "pcm.hpp"
#include "lib_utils/clock.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/tools.hpp"
#include <cassert>
#include <cstdio>
#include <mutex>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#undef PixelFormat
}

template<>
std::shared_ptr<AVCodecContext> shptr(AVCodecContext *p) {
	return std::shared_ptr<AVCodecContext>(p, Modules::AVCodecContextDeleter);
}

namespace {

int av_lockmgr(void **mutex, enum AVLockOp op) {
	if (NULL == mutex) {
		return -1;
	}

	switch (op) {
	case AV_LOCK_CREATE: {
		auto m = new std::mutex();
		*mutex = static_cast<void*>(m);
		break;
	}
	case AV_LOCK_OBTAIN: {
		auto m = static_cast<std::mutex*>(*mutex);
		m->lock();
		break;
	}
	case AV_LOCK_RELEASE: {
		auto m = static_cast<std::mutex*>(*mutex);
		m->unlock();
		break;
	}
	case AV_LOCK_DESTROY: {
		auto m = static_cast<std::mutex*>(*mutex);
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
		return Error;
	case AV_LOG_ERROR:
	case AV_LOG_WARNING:
		return Warning;
	case AV_LOG_INFO:
		return Info;
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

void avLog(void* /*avcl*/, int level, const char *fmt, va_list vl) {
#if defined(__CYGWIN__) // cygwin does not have vsnprintf in std=c++11 mode. To be removed when cygwin is fixed
	Log::msg(avLogLevel(level), "[libav-log::%s] %s", avlogLevelName(level), fmt);
#else
	char buffer[1280];
	std::vsnprintf(buffer, sizeof(buffer)-1, fmt, vl);

	// remove trailing end of line
	{
		auto const N = strlen(buffer);
		if (N > 0 && buffer[N-1] == '\n')
			buffer[N-1] = 0;
	}
	Log::msg(avLogLevel(level), "[libav-log::%s] %s", avlogLevelName(level), buffer);
#endif
}

auto g_InitAvcodec = runAtStartup(&avcodec_register_all);
auto g_InitAvdevice = runAtStartup(&avdevice_register_all);
auto g_InitAv = runAtStartup(&av_register_all);
auto g_InitAvnetwork = runAtStartup(&avformat_network_init);
auto g_InitAvfilter = runAtStartup(avfilter_register_all);
auto g_InitAvLog = runAtStartup(&av_log_set_callback, avLog);

}

namespace Modules {

void AVCodecContextDeleter(AVCodecContext *p) {
	avcodec_close(p);
	avcodec_free_context(&p);
}

MetadataPktLibav::MetadataPktLibav(std::shared_ptr<AVCodecContext> codecCtx, int id)
	: codecCtx(codecCtx), id(id) {
}

StreamType MetadataPktLibav::getStreamType() const {
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: return VIDEO_PKT;
	case AVMEDIA_TYPE_AUDIO: return AUDIO_PKT;
	case AVMEDIA_TYPE_SUBTITLE: return SUBTITLE_PKT;
	default: throw std::runtime_error("Unknown stream type. Only audio, video and subtitle handled.");
	}
}

std::shared_ptr<AVCodecContext> MetadataPktLibav::getAVCodecContext() const {
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

Fraction MetadataPktLibavVideo::getTimeScale() const {
	if (!codecCtx->time_base.den || !codecCtx->time_base.den)
		throw std::runtime_error(format("Unsupported video time scale %s/%s.", codecCtx->time_base.den, codecCtx->time_base.num));
	return Fraction(codecCtx->time_base.den, codecCtx->time_base.num);
}

Fraction MetadataPktLibavVideo::getFrameRate() const {
	if (!codecCtx->framerate.den || !codecCtx->framerate.den)
		throw std::runtime_error(format("Unsupported video frame rate %s/%s.", codecCtx->framerate.den, codecCtx->framerate.num));
	return Fraction(codecCtx->framerate.num, codecCtx->framerate.den);
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

void libavAudioCtx2pcmConvert(std::shared_ptr<const AVCodecContext> codecCtx, PcmFormat *cfg) {
	cfg->sampleRate = codecCtx->sample_rate;

	cfg->numChannels = cfg->numPlanes = codecCtx->channels;
	switch (codecCtx->sample_fmt) {
	case AV_SAMPLE_FMT_S16:
		cfg->sampleFormat = Modules::S16;
		cfg->numPlanes = 1;
		break;
	case AV_SAMPLE_FMT_S16P:
		cfg->sampleFormat = Modules::S16;
		break;
	case AV_SAMPLE_FMT_FLT:
		cfg->sampleFormat = Modules::F32;
		cfg->numPlanes = 1;
		break;
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
	case AV_SAMPLE_FMT_S16:
		cfg->sampleFormat = Modules::S16;
		cfg->numPlanes = 1;
		break;
	case AV_SAMPLE_FMT_S16P:
		cfg->sampleFormat = Modules::S16;
		break;
	case AV_SAMPLE_FMT_FLT:
		cfg->sampleFormat = Modules::F32;
		cfg->numPlanes = 1;
		break;
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
	case YUV420P10LE: avPixfmt = AV_PIX_FMT_YUV420P10LE; break;
	case YUV422P: avPixfmt = AV_PIX_FMT_YUV422P; break;
	case YUYV422: avPixfmt = AV_PIX_FMT_YUYV422; break;
	case NV12: avPixfmt = AV_PIX_FMT_NV12; break;
	case RGB24: avPixfmt = AV_PIX_FMT_RGB24; break;
	case RGBA32: avPixfmt = AV_PIX_FMT_RGBA; break;
	default: throw std::runtime_error("Unknown pixel format to convert (1). Please contact your vendor.");
	}
}

enum PixelFormat libavPixFmt2PixelFormat(const AVPixelFormat &avPixfmt) {
	switch (avPixfmt) {
	case AV_PIX_FMT_YUV420P: case AV_PIX_FMT_YUVJ420P: return YUV420P;
	case AV_PIX_FMT_YUV420P10LE: return YUV420P10LE;
	case AV_PIX_FMT_YUV422P: return YUV422P;
	case AV_PIX_FMT_YUYV422: return YUYV422;
	case AV_PIX_FMT_NV12: return NV12;
	case AV_PIX_FMT_RGB24: return RGB24;
	case AV_PIX_FMT_RGBA: return RGBA32;
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

void copyToPicture(AVFrame const* avFrame, DataPicture* pic) {
	for (size_t comp = 0; comp<pic->getNumPlanes(); ++comp) {
		auto const subsampling = comp == 0 ? 1 : 2;
		auto const bytePerPixel = pic->getFormat().format == YUYV422 ? 2 : 1;
		auto const w = avFrame->width * bytePerPixel / subsampling;
		auto const h = avFrame->height / subsampling;
		auto src = avFrame->data[comp];
		auto const srcPitch = avFrame->linesize[comp];
		auto dst = pic->getPlane(comp);
		auto const dstPitch = pic->getPitch(comp);
		for (int y = 0; y<h; ++y) {
			memcpy(dst, src, w);
			src += srcPitch;
			dst += dstPitch;
		}
	}
}

static void lavc_ReleaseFrame(void *opaque, uint8_t *data) {
	if (opaque) {
		delete static_cast<LibavDirectRendering::LibavDirectRenderingContext*>(opaque);
	}
}

int avGetBuffer2(struct AVCodecContext *ctx, AVFrame *frame, int flags) {
	auto dr = static_cast<LibavDirectRendering*>(ctx->opaque);
	int width = frame->width;
	int height = frame->height;
	int linesize_align[AV_NUM_DATA_POINTERS];
	avcodec_align_dimensions2(ctx, &width, &height, linesize_align);
	if (width % (2 * linesize_align[0])) {
		width += 2 * linesize_align[0] - (width % (2 * linesize_align[0]));
	}
	auto picCtx = dr->getPicture(Resolution(frame->width, frame->height), Resolution(width, height), libavPixFmt2PixelFormat((AVPixelFormat)frame->format));
	if (!picCtx->pic)
		return -1;
	frame->opaque = static_cast<void*>(picCtx);
	auto pic = picCtx->pic.get();

	for (size_t i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
		frame->data[i] = NULL;
		frame->linesize[i] = 0;
		frame->buf[i] = NULL;
	}
	for (size_t i = 0; i < pic->getNumPlanes(); ++i) {
		frame->data[i] = pic->getPlane(i);
		frame->linesize[i] = (int)pic->getPitch(i);
		assert(!(pic->getPitch(i) % linesize_align[i]));
		frame->buf[i] = av_buffer_create(frame->data[i], frame->linesize[i], lavc_ReleaseFrame, i == 0 ? (void*)picCtx : nullptr, 0);
	}

	return 0;
}

}
