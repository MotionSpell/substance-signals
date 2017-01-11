#include "libav_encode.hpp"
#include "lib_utils/tools.hpp"
#include "../common/pcm.hpp"
#include <cassert>

#include "lib_ffpp/ffpp.hpp"


namespace Modules {

namespace {
void fps2NumDen(const double fps, int &num, int &den) {
	if (fabs(fps - (int)fps) < 0.001) {
		//infer integer frame rates
		num = (int)fps;
		den = 1;
	} else if (fabs((fps*1001.0) / 1000.0 - (int)(fps + 1)) < 0.001) {
		//infer ATSC frame rates
		num = (int)(fps + 1) * 1000;
		den = 1001;
	} else if (fabs(fps * 2 - (int)(fps * 2)) < 0.001) {
		//infer rational frame rates; den = 2
		num = (int)(fps * 2);
		den = 2;
	} else if (fabs(fps * 4 - (int)(fps * 4)) < 0.001) {
		//infer rational frame rates; den = 4
		num = (int)(fps * 4);
		den = 4;
	} else {
		num = (int)fps;
		den = 1;
		Log::msg(Warning, "Frame rate '%s' was not recognized. Truncating to '%s'.", fps, num);
	}
}

auto g_InitAv = runAtStartup(&av_register_all);
auto g_InitAvcodec = runAtStartup(&avcodec_register_all);
auto g_InitAvLog = runAtStartup(&av_log_set_callback, avLog);
}

namespace Encode {

LibavEncode::LibavEncode(Type type, LibavEncodeParams &params)
	: avFrame(new ffpp::Frame) {
	std::string codecOptions, generalOptions, codecName;
	switch (type) {
	case Video:
		switch (params.codecType) {
		case LibavEncodeParams::Software:
			generalOptions = " -vcodec libx264";
			if (params.isLowLatency) {
				codecOptions += " -preset ultrafast -tune zerolatency";
			} else {
				codecOptions += " -preset veryfast";
			}
			break;
		case LibavEncodeParams::Hardware_qsv:
			generalOptions = " -vcodec h264_qsv";
			break;
		case LibavEncodeParams::Hardware_nvenc:
			generalOptions = " -vcodec nvenc_h264";
			break;
		default:
			throw error("Unknown video encoder type. Failed.");
		}
		generalOptions += format(" -r %s -pass 1", params.frameRate);
		codecOptions += format(" -b %s -g %s -keyint_min %s -bf 0 -sc_threshold 0", params.bitrate_v, params.GOPSize, params.GOPSize);
		codecName = "vcodec";
		break;
	case Audio:
		codecOptions = format(" -b %s -ar %s -ac %s", params.bitrate_a, params.sampleRate, params.numChannels);
		generalOptions = " -acodec aac";
		codecName = "acodec";
		break;
	default:
		throw error("Unknown encoder type. Failed.");
	}

	/* parse the codec optionsDict */
	ffpp::Dict codecDict(typeid(*this).name(), "codec", codecOptions + "-threads auto" + params.avcodecCustom);

	/* parse other optionsDict*/
	ffpp::Dict generalDict(typeid(*this).name(), "other", generalOptions);

	/* find the encoder */
	auto entry = generalDict.get(codecName);
	if (!entry)
		throw error("Could not get codecName.");
	AVCodec *codec = avcodec_find_encoder_by_name(entry->value);
	if (!codec)
		throw error(format("codec '%s' not found, disable output.", entry->value));

	codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx)
		throw error("could not allocate the codec context.");

	/* parameters */
	switch (type) {
	case Video: {
		codecCtx->width = params.res.width;
		codecCtx->height = params.res.height;
		if (!strcmp(generalDict.get("vcodec")->value, "mjpeg")) {
			codecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
		} else if (!strcmp(generalDict.get("vcodec")->value, "h264_qsv")) {
			codecCtx->pix_fmt = AV_PIX_FMT_NV12;
		} else {
			codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
		}
		params.pixelFormat = libavPixFmt2PixelFormat(codecCtx->pix_fmt);

		double fr = atof(generalDict.get("r")->value);
		AVRational fps;
		fps2NumDen(fr, fps.den, fps.num); //for FPS, num and den are inverted
		codecCtx->time_base = fps;
	}
	break;
	case Audio:
		AudioLayout layout;
		switch (params.numChannels) {
		case 1: layout = Modules::Mono; break;
		case 2: layout = Modules::Stereo; break;
		default: throw error("Unknown libav audio layout");
		}
		pcmFormat = uptr(new PcmFormat(params.sampleRate, params.numChannels, layout));
		libavAudioCtxConvert(pcmFormat.get(), codecCtx);
		break;
	default:
		assert(0);
	}

	/* open it */
	codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //gives access to the extradata (e.g. H264 SPS/PPS, etc.)
	if (avcodec_open2(codecCtx, codec, &codecDict) < 0)
		throw error("could not open codec, disable output.");
	codecDict.ensureAllOptionsConsumed();

	output = addOutput<OutputDataDefault<DataAVPacket>>();
	switch (type) {
	case Video: {
		auto input = addInput(new Input<DataPicture>(this));
		input->setMetadata(new MetadataRawVideo);
		output->setMetadata(new MetadataPktLibavVideo(codecCtx));
		break;
	}
	case Audio: {
		auto input = addInput(new Input<DataPcm>(this));
		input->setMetadata(new MetadataRawAudio);
		output->setMetadata(new MetadataPktLibavAudio(codecCtx));
		break;
	}
	default:
		assert(0);
	}

	av_dict_free(&generalDict);
	avFrame->get()->pts = -1;
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
	if (codecCtx) {
		avcodec_close(codecCtx);
		avcodec_free_context(&codecCtx);
	}
}

bool LibavEncode::processAudio(const DataPcm *data) {
	auto out = output->getBuffer(0);
	AVPacket *pkt = out->getPacket();
	AVFrame *f = nullptr;
	if (data) {
		f = avFrame->get();
		libavFrameDataConvert(data, f);
		f->pts++;
	}

	int gotPkt = 0;
	if (avcodec_encode_audio2(codecCtx, pkt, f, &gotPkt)) {
		log(Warning, "error encountered while encoding audio frame %s.", f ? f->pts : -1);
		return false;
	}
	if (gotPkt) {
		pkt->pts = pkt->dts = cumulatedPktDuration;
		cumulatedPktDuration += pkt->duration;
		if (pkt->duration != codecCtx->frame_size) {
			log(Warning, "pkt duration %s is different from codec frame size %s - this may cause timing errors", pkt->duration, codecCtx->frame_size);
		}
		uint64_t time;
		if (times.tryPop(time)) {
			out->setTime(time);
		} else {
			log(Warning, "error encountered at input frame %s, output dts %s: more output packets than input. Discard", f ? f->pts : -1, pkt->dts);
			return false;
		}
		assert(pkt->size);
		if (out) output->emit(out);
		return true;
	}

	return false;
}

bool LibavEncode::processVideo(const DataPicture *pic) {
	auto out = output->getBuffer(0);
	AVPacket *pkt = out->getPacket();

	AVFrame *f = nullptr;
	if (pic) {
		f = avFrame->get();
		f->pict_type = AV_PICTURE_TYPE_NONE;
		AVPixelFormat avpf; pixelFormat2libavPixFmt(pic->getFormat().format, avpf); f->format = (int)avpf;
		for (size_t i = 0; i < pic->getNumPlanes(); ++i) {
			f->width = pic->getFormat().res.width;
			f->height = pic->getFormat().res.height;
			f->data[i] = (uint8_t*)pic->getPlane(i);
			f->linesize[i] = (int)pic->getPitch(i);
		}
		f->pts++;
	}

	int gotPkt = 0;
	if (avcodec_encode_video2(codecCtx, pkt, f, &gotPkt)) {
		log(Warning, "error encountered while encoding video frame %s.", f ? f->pts : -1);
		return false;
	} else {
		if (gotPkt) {
			assert(pkt->size);
			if (pkt->duration <= 0) {
				pkt->duration = 1; /*store duration in case a forward module (e.g. muxer) would need it*/
			}
			out->setTime(times.pop());
			output->emit(out);
			return true;
		}
	}

	return false;
}

void LibavEncode::process(Data data) {
	times.push(data->getTime());
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: {
		const auto encoderData = safe_cast<const DataPicture>(data);
		processVideo(encoderData.get());
		break;
	}
	case AVMEDIA_TYPE_AUDIO: {
		const auto pcmData = safe_cast<const DataPcm>(data);
		if (pcmData->getFormat() != *pcmFormat)
			throw error("Incompatible audio data");
		processAudio(pcmData.get());
		break;
	}
	default:
		assert(0);
		return;
	}
}

}
}
