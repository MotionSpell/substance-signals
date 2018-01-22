#include "libav_encode.hpp"
#include "lib_utils/tools.hpp"
#include "lib_ffpp/ffpp.hpp"
#include "../common/pcm.hpp"
#include <cassert>

extern "C" {
#include <libavutil/pixdesc.h>
}

#define TIMESCALE_MUL 100

namespace Modules {
namespace Encode {

LibavEncode::LibavEncode(Type type, Params &params)
: avFrame(new ffpp::Frame) {
	std::string codecOptions, generalOptions, codecName;
	switch (type) {
	case Video: {
		codecName = "vcodec";
		ffpp::Dict customDict(typeid(*this).name(), params.avcodecCustom);
		auto const pixFmt = customDict.get("pix_fmt");
		if (pixFmt) {
			generalOptions += format(" -pix_fmt %s", pixFmt->value);
		}
		auto const codec = customDict.get("vcodec");
		if (codec) {
			generalOptions += format(" -vcodec %s", codec->value);
			av_dict_free(&customDict);
			break;
		}
		av_dict_free(&customDict);
		codecOptions += " -forced-idr 1";
		switch (params.codecType) {
		case Software:
			generalOptions += " -vcodec libx264";
			if (params.isLowLatency) {
				codecOptions += " -preset ultrafast -tune zerolatency";
			} else {
				codecOptions += " -preset veryfast";
			}
			break;
		case Hardware_qsv:
			generalOptions += " -vcodec h264_qsv";
			break;
		case Hardware_nvenc:
			generalOptions += " -vcodec h264_nvenc";
			break;
		default:
			throw error("Unknown video encoder type. Failed.");
		}
		codecOptions += format(" -b %s -bf 0", params.bitrate_v);
		GOPSize = params.GOPSize;
		break;
	}
	case Audio: {
		codecName = "acodec";
		ffpp::Dict customDict(typeid(*this).name(), params.avcodecCustom);
		auto const codec = customDict.get("acodec");
		if (codec) {
			generalOptions += format(" -acodec %s", codec->value);
			av_dict_free(&customDict);
			break;
		}
		av_dict_free(&customDict);
		codecOptions += format(" -b %s -ar %s -ac %s", params.bitrate_a, params.sampleRate, params.numChannels);
		generalOptions += " -acodec aac -profile aac_low";
		GOPSize = 1;
		break;
	}
	default:
		throw error("Unknown encoder type. Failed.");
	}

	/* find the encoder */
	ffpp::Dict generalDict(typeid(*this).name(), generalOptions);
	auto const entry = generalDict.get(codecName);
	if (!entry)
		throw error(format("Could not get codecName (\"%s\").", codecName));
	auto codec = avcodec_find_encoder_by_name(entry->value);
	if (!codec) {
		auto desc = avcodec_descriptor_get_by_name(entry->value);
		if (!desc)
			throw error(format("Codec descriptor '%s' not found, disable output.", entry->value));
		codec = avcodec_find_encoder(desc->id);
	}
	if (!codec)
		throw error(format("Codec '%s' not found, disable output.", entry->value));
	codecCtx = shptr(avcodec_alloc_context3(codec));
	if (!codecCtx)
		throw error(format("Could not allocate the codec context (%s).", codecName));

	/* parameters */
	switch (type) {
	case Video: {
		codecCtx->width = params.res.width;
		codecCtx->height = params.res.height;
		if (generalDict.get("pix_fmt")) {
			codecCtx->pix_fmt = av_get_pix_fmt(generalDict.get("pix_fmt")->value);
			if (codecCtx->pix_fmt == AV_PIX_FMT_NONE)
				throw error(format("Unknown pixel format\"%s\".", generalDict.get("pix_fmt")->value));
			av_dict_set(&generalDict, "pix_fmt", nullptr, 0);
		} else if (!strcmp(generalDict.get("vcodec")->value, "mjpeg")) {
			codecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
		} else if (!strcmp(generalDict.get("vcodec")->value, "h264_qsv")) {
			codecCtx->pix_fmt = AV_PIX_FMT_NV12;
		} else {
			codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
		}
		params.pixelFormat = libavPixFmt2PixelFormat(codecCtx->pix_fmt);

		AVRational fps;
		fps2NumDen((double)params.frameRate, fps.den, fps.num); //for time_base, 'num' and 'den' are inverted
		fps.den *= TIMESCALE_MUL;
		codecCtx->time_base = fps;
		codecCtx->ticks_per_frame *= TIMESCALE_MUL;
		break;
	}
	case Audio:
		AudioLayout layout;
		switch (params.numChannels) {
		case 1: layout = Modules::Mono; break;
		case 2: layout = Modules::Stereo; break;
		default: throw error("Unknown libav audio layout");
		}
		pcmFormat = uptr(new PcmFormat(params.sampleRate, params.numChannels, layout));
		libavAudioCtxConvert(pcmFormat.get(), codecCtx.get());
		break;
	default:
		assert(0);
	}

	/* open it */
	codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //gives access to the extradata (e.g. H264 SPS/PPS, etc.)
	ffpp::Dict codecDict(typeid(*this).name(), codecOptions + " -threads auto " + params.avcodecCustom);
	av_dict_set(&codecDict, codecName.c_str(), nullptr, 0);
	if (avcodec_open2(codecCtx.get(), codec, &codecDict) < 0)
		throw error(format("Could not open codec %s, disable output.", codecName));
	codecDict.ensureAllOptionsConsumed();

	output = addOutput<OutputDataDefault<DataAVPacket>>();
	switch (type) {
	case Video: {
		auto input = addInput(new Input<DataPicture>(this));
		input->setMetadata(shptr(new MetadataRawVideo));
		output->setMetadata(shptr(new MetadataPktLibavVideo(codecCtx)));
		break;
	}
	case Audio: {
		auto input = addInput(new Input<DataPcm>(this));
		input->setMetadata(shptr(new MetadataRawAudio));
		output->setMetadata(shptr(new MetadataPktLibavAudio(codecCtx)));
		break;
	}
	default:
		assert(0);
	}

	av_dict_free(&generalDict);
	avFrame->get()->pts = std::numeric_limits<int64_t>::min();
}

void LibavEncode::flush() {
	if (codecCtx && (codecCtx->codec->capabilities & CODEC_CAP_DELAY)) {
		switch (codecCtx->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
			while (processVideo(nullptr)) {}
			break;
		case AVMEDIA_TYPE_AUDIO:
			while (processAudio(nullptr)) {}
			break;
		default:
			assert(0);
			break;
		}
	}
}

LibavEncode::~LibavEncode() {
}

void LibavEncode::computeDurationAndEmit(std::shared_ptr<DataAVPacket> &data, int64_t defaultDuration) {
	auto pkt = data->getPacket();
	assert(pkt->size);
	if (pkt->duration != (int64_t)(pkt->dts - lastDTS)) {
		log(Debug, "VFR detected: duration is %s but timestamp diff is %s", pkt->duration, pkt->dts - lastDTS);
		pkt->duration = pkt->dts - lastDTS;
	}
	lastDTS = pkt->dts;
	if (pkt->duration <= 0) {
		pkt->duration = defaultDuration;
	}
	data->setMediaTime(pkt->dts * codecCtx->time_base.num, codecCtx->time_base.den);
	output->emit(data);
}

int64_t LibavEncode::computePTS(const int64_t mediaTime) const {
	return (mediaTime * codecCtx->time_base.den) / (codecCtx->time_base.num * (int64_t)Clock::Rate);
}

bool LibavEncode::processAudio(const DataPcm *data) {
	auto out = output->getBuffer(0);
	AVPacket *pkt = out->getPacket();
	AVFrame *f = nullptr;
	if (data) {
		f = avFrame->get();
		libavFrameDataConvert(data, f);
		f->pts = computePTS(data->getMediaTime());
	}

	int gotPkt = 0;
	if (avcodec_encode_audio2(codecCtx.get(), pkt, f, &gotPkt)) {
		log(Warning, "error encountered while encoding audio frame %s.", f ? f->pts : std::numeric_limits<int64_t>::min());
		return false;
	} else if (gotPkt) {
		if (pkt->duration != codecCtx->frame_size) {
			log(Warning, "pkt duration %s is different from codec frame size %s - this may cause timing errors", pkt->duration, codecCtx->frame_size);
		}
		computeDurationAndEmit(out, pkt->duration);
		return true;
	} else {
		return false;
	}
}

void LibavEncode::computeFrameAttributes(AVFrame * const f, const int64_t currMediaTime) {
	if (f->pts == std::numeric_limits<int64_t>::min()) {
		firstMediaTime = currMediaTime;
		f->key_frame = 1;
		f->pict_type = AV_PICTURE_TYPE_I;
	} else {
		auto const prevGOP = ((prevMediaTime - firstMediaTime) * GOPSize.den * codecCtx->time_base.den) / (GOPSize.num * codecCtx->time_base.num * TIMESCALE_MUL * (int64_t)Clock::Rate);
		auto const currGOP = ((currMediaTime - firstMediaTime) * GOPSize.den * codecCtx->time_base.den) / (GOPSize.num * codecCtx->time_base.num * TIMESCALE_MUL * (int64_t)Clock::Rate);
		if (prevGOP != currGOP) {
			if (currGOP != prevGOP + 1) {
				log(Warning, "Invalid content: switching from GOP %s to GOP %s - inserting RAP.", prevGOP, currGOP);
			}
			f->key_frame = 1;
			f->pict_type = AV_PICTURE_TYPE_I;
		} else {
			f->key_frame = 0;
			f->pict_type = AV_PICTURE_TYPE_NONE;
		}
	}
	prevMediaTime = currMediaTime;
}

bool LibavEncode::processVideo(const DataPicture *pic) {
	auto out = output->getBuffer(0);
	AVFrame *f = nullptr;
	if (pic) {
		f = avFrame->get();
		AVPixelFormat avpf; pixelFormat2libavPixFmt(pic->getFormat().format, avpf); f->format = (int)avpf;
		for (size_t i = 0; i < pic->getNumPlanes(); ++i) {
			f->width = pic->getFormat().res.width;
			f->height = pic->getFormat().res.height;
			f->data[i] = (uint8_t*)pic->getPlane(i);
			f->linesize[i] = (int)pic->getPitch(i);
		}
		computeFrameAttributes(f, pic->getMediaTime());
		f->pts = computePTS(pic->getMediaTime());
	}

	int gotPkt = 0;
	AVPacket *pkt = out->getPacket();
	if (avcodec_encode_video2(codecCtx.get(), pkt, f, &gotPkt)) {
		log(Warning, "error encountered while encoding video frame %s.", f ? f->pts : std::numeric_limits<int64_t>::min());
		return false;
	} else if (gotPkt) {
		computeDurationAndEmit(out, TIMESCALE_MUL);
		return true;
	} else {
		return false;
	}
}

void LibavEncode::process(Data data) {
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: {
		const auto encoderData = safe_cast<const DataPicture>(data);
		processVideo(encoderData.get());
		break;
	}
	case AVMEDIA_TYPE_AUDIO: {
		const auto pcmData = safe_cast<const DataPcm>(data);
		if (pcmData->getFormat() != *pcmFormat)
			throw error("Incompatible audio data (1)");
		processAudio(pcmData.get());
		break;
	}
	default:
		assert(0); return;
	}
}

}
}
