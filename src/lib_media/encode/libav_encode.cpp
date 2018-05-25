#include "libav_encode.hpp"
#include "lib_utils/tools.hpp"
#include "lib_ffpp/ffpp.hpp"
#include "../common/pcm.hpp"
#include <cassert>

extern "C" {
#include <libavutil/pixdesc.h>
}

using std::make_shared;

#define TIMESCALE_MUL 100

namespace Modules {
namespace Encode {

LibavEncode::LibavEncode(Type type, Params &params)
	: avFrame(new ffpp::Frame) {
	std::string codecOptions, generalOptions, codecName;
	switch (type) {
	case Video: {
		GOPSize = params.GOPSize;
		codecOptions += format(" -b %s", params.bitrate_v);
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
		codecOptions += " -bf 0";
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
		throw error(format("Could not allocate the codec context (\"%s\").", codecName));

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
		input->setMetadata(make_shared<MetadataRawVideo>());
		output->setMetadata(make_shared<MetadataPktLibavVideo>(codecCtx));
		break;
	}
	case Audio: {
		auto input = addInput(new Input<DataPcm>(this));
		input->setMetadata(make_shared<MetadataRawAudio>());
		output->setMetadata(make_shared<MetadataPktLibavAudio>(codecCtx));
		break;
	}
	default:
		assert(0);
	}

	av_dict_free(&generalDict);
	avFrame->get()->pts = std::numeric_limits<int64_t>::min();
}

void LibavEncode::flush() {
	if (codecCtx && (codecCtx->codec->capabilities & AV_CODEC_CAP_DELAY)) {
		encodeFrame(nullptr);
	}
}

LibavEncode::~LibavEncode() {
}

void LibavEncode::computeDurationAndEmit(std::shared_ptr<DataAVPacket> &data, int64_t defaultDuration) {
	auto pkt = data->getPacket();
	if (!pkt->size) {
		log(Warning, "Incorrect zero-sized packet");
		return;
	}
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
	return (mediaTime * codecCtx->time_base.den) / (codecCtx->time_base.num * (int64_t)IClock::Rate);
}

bool LibavEncode::processAudio(Data data) {
	AVFrame *f = nullptr;
	if (data) {
		const auto pcmData = safe_cast<const DataPcm>(data).get();
		if (pcmData->getFormat() != *pcmFormat)
			throw error("Incompatible audio data (1)");
		f = avFrame->get();
		libavFrameDataConvert(pcmData, f);
		f->pts = computePTS(data->getMediaTime());
	}

	encodeFrame(f);
	return true;
}

void LibavEncode::computeFrameAttributes(AVFrame * const f, const int64_t currMediaTime) {
	if (f->pts == std::numeric_limits<int64_t>::min()) {
		firstMediaTime = currMediaTime;
		f->key_frame = 1;
		f->pict_type = AV_PICTURE_TYPE_I;
	} else {
		auto const prevGOP = ((prevMediaTime - firstMediaTime) * GOPSize.den * codecCtx->time_base.den) / (GOPSize.num * codecCtx->time_base.num * TIMESCALE_MUL * (int64_t)IClock::Rate);
		auto const currGOP = ((currMediaTime - firstMediaTime) * GOPSize.den * codecCtx->time_base.den) / (GOPSize.num * codecCtx->time_base.num * TIMESCALE_MUL * (int64_t)IClock::Rate);
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

bool LibavEncode::processVideo(Data data) {
	const auto pic = safe_cast<const DataPicture>(data).get();
	AVFrame *f = nullptr;
	if (pic) {
		f = avFrame->get();
		f->format = (int)pixelFormat2libavPixFmt(pic->getFormat().format);
		for (size_t i = 0; i < pic->getNumPlanes(); ++i) {
			f->width = pic->getFormat().res.width;
			f->height = pic->getFormat().res.height;
			f->data[i] = (uint8_t*)pic->getPlane(i);
			f->linesize[i] = (int)pic->getPitch(i);
		}
		computeFrameAttributes(f, data->getMediaTime());
		f->pts = computePTS(data->getMediaTime());
	}

	encodeFrame(f);
	return true;
}

void LibavEncode::encodeFrame(AVFrame* f) {
	int ret;

	ret = avcodec_send_frame(codecCtx.get(), f);
	if (ret != 0) {
		auto desc = f ? format("pts=%s", f->pts) : format("flush");
		log(Warning, "error encountered while encoding frame (%s) : %s", desc, avStrError(ret));
		return;
	}

	while(1) {
		auto out = output->getBuffer(0);
		AVPacket *pkt = out->getPacket();
		ret = avcodec_receive_packet(codecCtx.get(), pkt);
		if(ret != 0)
			break;

		if (pkt->duration != codecCtx->frame_size) {
			log(Warning, "pkt duration %s is different from codec frame size %s - this may cause timing errors", pkt->duration, codecCtx->frame_size);
		}

		computeDurationAndEmit(out, TIMESCALE_MUL);
	}
}

void LibavEncode::process(Data data) {
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: processVideo(data); break;
	case AVMEDIA_TYPE_AUDIO: processAudio(data); break;
	default: assert(0); return;
	}
}

}
}
