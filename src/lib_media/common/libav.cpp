#include "libav.hpp"
#include "pcm.hpp"
#include "picture_allocator.hpp"
#include "lib_utils/clock.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/tools.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
}

namespace Modules {
void AVCodecContextDeleter(AVCodecContext *p);
}

std::shared_ptr<AVCodecContext> shptr(AVCodecContext *p) {
	return std::shared_ptr<AVCodecContext>(p, Modules::AVCodecContextDeleter);
}


namespace Modules {

void AVCodecContextDeleter(AVCodecContext *p) {
	avcodec_close(p);
	avcodec_free_context(&p);
}

StreamType getType(AVCodecContext* codecCtx) {
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: return VIDEO_PKT;
	case AVMEDIA_TYPE_AUDIO: return AUDIO_PKT;
	case AVMEDIA_TYPE_SUBTITLE: return SUBTITLE_PKT;
	default: throw std::runtime_error("Unknown stream type. Only audio, video and subtitle handled.");
	}
}

MetadataPktLibav::MetadataPktLibav(std::shared_ptr<AVCodecContext> codecCtx)
	: MetadataPkt(getType(codecCtx.get())), codecCtx(codecCtx) {
	enforce(codecCtx != nullptr, "MetadataPktLibav 'codecCtx' can't be null.");
	codec = avcodec_get_name(codecCtx->codec_id);
	codecSpecificInfo.assign(codecCtx->extradata, codecCtx->extradata + codecCtx->extradata_size);

}

std::shared_ptr<AVCodecContext> MetadataPktLibav::getAVCodecContext() const {
	return codecCtx;
}

int64_t MetadataPktLibav::getBitrate() const {
	return codecCtx->bit_rate;
}

Fraction MetadataPktLibav::getTimeScale() const {
	if (!codecCtx->time_base.num || !codecCtx->time_base.den)
		throw std::runtime_error(format("Unsupported time scale %s/%s.", codecCtx->time_base.den, codecCtx->time_base.num));
	return Fraction(codecCtx->time_base.den, codecCtx->time_base.num);
}

std::string MetadataPktLibav::getCodecName() const {
	return codec;
}

PixelFormat MetadataPktLibavVideo::getPixelFormat() const {
	return libavPixFmt2PixelFormat(codecCtx->pix_fmt);
}

Fraction MetadataPktLibavVideo::getSampleAspectRatio() const {
	auto const &sar = codecCtx->sample_aspect_ratio;
	return Fraction(sar.num, sar.den);
}

Resolution MetadataPktLibavVideo::getResolution() const {
	return Resolution(codecCtx->width, codecCtx->height);
}

Fraction MetadataPktLibavVideo::getFrameRate() const {
	if (!codecCtx->framerate.num || !codecCtx->framerate.den)
		throw std::runtime_error(format("Unsupported video frame rate %s/%s.", codecCtx->framerate.den, codecCtx->framerate.num));
	return Fraction(codecCtx->framerate.num, codecCtx->framerate.den);
}

Span MetadataPktLibavVideo::getExtradata() const {
	return Span { codecCtx->extradata, (size_t)codecCtx->extradata_size };
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

Span MetadataPktLibavAudio::getExtradata() const {
	return Span { codecCtx->extradata, (size_t)codecCtx->extradata_size };
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

PcmFormat toPcmFormat(std::shared_ptr<const MetadataPktLibavAudio> meta) {
	PcmFormat cfg_;
	PcmFormat *cfg = &cfg_;
	auto codecCtx = meta->getAVCodecContext();
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
		throw std::runtime_error(format("Unknown libav audio format [%s] (2)", codecCtx->sample_fmt));
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

	return *cfg;
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
	for (int i = 0; i < format.numPlanes; ++i) {
		frame->data[i] = pcmData->getPlane(i);
		if (i == 0)
			frame->linesize[i] = (int)pcmData->getPlaneSize(i) / format.numChannels;
		else
			frame->linesize[i] = 0;
	}
	frame->nb_samples = (int)(pcmData->size() / format.getBytesPerSample());
}

AVPixelFormat pixelFormat2libavPixFmt(PixelFormat format) {
	switch (format) {
	case Y8: return AV_PIX_FMT_GRAY8;
	case YUV420P: return AV_PIX_FMT_YUV420P;
	case YUV420P10LE: return AV_PIX_FMT_YUV420P10LE;
	case YUV422P: return AV_PIX_FMT_YUV422P;
	case YUV422P10LE: return AV_PIX_FMT_YUV422P10LE;
	case YUYV422: return AV_PIX_FMT_YUYV422;
	case NV12: return AV_PIX_FMT_NV12;
	case NV12P010LE: return AV_PIX_FMT_P010LE;
	case RGB24: return AV_PIX_FMT_RGB24;
	case RGBA32: return AV_PIX_FMT_RGBA;
	default: throw std::runtime_error("Unknown pixel format to convert (1). Please contact your vendor.");
	}
}

PixelFormat libavPixFmt2PixelFormat(AVPixelFormat avPixfmt) {
	switch (avPixfmt) {
	case AV_PIX_FMT_GRAY8: return Y8;
	case AV_PIX_FMT_YUV420P: case AV_PIX_FMT_YUVJ420P: return YUV420P;
	case AV_PIX_FMT_YUV420P10LE: return YUV420P10LE;
	case AV_PIX_FMT_YUV422P: return YUV422P;
	case AV_PIX_FMT_YUV422P10LE: return YUV422P10LE;
	case AV_PIX_FMT_YUYV422: return YUYV422;
	case AV_PIX_FMT_NV12: return NV12;
	case AV_PIX_FMT_P010LE: return NV12P010LE;
	case AV_PIX_FMT_RGB24: return RGB24;
	case AV_PIX_FMT_RGBA: return RGBA32;
	default: throw std::runtime_error("Unknown pixel format to convert (2). Please contact your vendor.");
	}
}

//DataAVPacket
void AVPacketDeleter::operator()(AVPacket *p) {
	av_packet_unref(p);
	delete(p);
}

DataAVPacket::DataAVPacket(size_t size)
	: pkt(std::unique_ptr<AVPacket, AVPacketDeleter>(new AVPacket)) {
	av_init_packet(pkt.get());
	av_packet_unref(pkt.get());
	if (size)
		av_new_packet(pkt.get(), (int)size);
}

DataAVPacket::~DataAVPacket() {
	g_Log->log(Debug, format("Freeing %s, pts=%s", this, pkt->pts).c_str());
}

Span DataAVPacket::data() {
	return Span { pkt->data, (size_t)pkt->size };
}

SpanC DataAVPacket::data() const {
	return SpanC { pkt->data, (size_t)pkt->size };
}

AVPacket* DataAVPacket::getPacket() const {
	return pkt.get();
}

void DataAVPacket::restamp(int64_t offsetIn180k, uint64_t pktTimescale) const {
	auto const offset = clockToTimescale(offsetIn180k, pktTimescale);
	pkt->dts += offset;
	if (pkt->pts != AV_NOPTS_VALUE) {
		pkt->pts += offset;
	}
}

bool DataAVPacket::isRap() const {
	return (pkt->flags & AV_PKT_FLAG_KEY) ? 1 : 0;
}

void DataAVPacket::resize(size_t size) {
	if (av_grow_packet(pkt.get(), size))
		throw std::runtime_error(format("Cannot resize DataAVPacket to size %s (cur=%s)", size, pkt->size));
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

static void lavc_ReleaseFrame(void *opaque, uint8_t * /*data*/) {
	delete static_cast<PictureAllocator::PictureContext*>(opaque);
}

int avGetBuffer2(struct AVCodecContext *ctx, AVFrame *frame, int /*flags*/) {
	try {
		auto dr = static_cast<PictureAllocator*>(ctx->opaque);
		auto dim = Resolution(frame->width, frame->height);
		auto size = dim; // size in memory
		int linesize_align[AV_NUM_DATA_POINTERS];
		avcodec_align_dimensions2(ctx, &size.width, &size.height, linesize_align);
		if (auto extra = size.width % (2 * linesize_align[0])) {
			size.width += 2 * linesize_align[0] - extra;
		}
		auto picCtx = dr->getPicture(dim, size, libavPixFmt2PixelFormat((AVPixelFormat)frame->format));
		if (!picCtx->pic)
			return -1;
		frame->opaque = picCtx;
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
	} catch(...) {
		return -1;
	}
}

std::string avStrError(int err) {
	char buffer[256] {};
	av_strerror(err, buffer, sizeof buffer);
	return buffer;
}

}
