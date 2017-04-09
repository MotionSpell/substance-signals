#include "libav_decode.hpp"
#include "../common/pcm.hpp"
#include "lib_modules/core/clock.hpp"
#include "lib_utils/tools.hpp"
#include "lib_ffpp/ffpp.hpp"
#include <cassert>

namespace Modules {

namespace {
auto g_InitAv = runAtStartup(&av_register_all);
auto g_InitAvcodec = runAtStartup(&avcodec_register_all);
auto g_InitAvLog = runAtStartup(&av_log_set_callback, avLog);
}

namespace Decode {

LibavDecode::LibavDecode(const MetadataPktLibav &metadata)
	: codecCtx(avcodec_alloc_context3(nullptr)), avFrame(new ffpp::Frame) {
	avcodec_copy_context(codecCtx, metadata.getAVCodecContext());

	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: break;
	case AVMEDIA_TYPE_AUDIO: break;
	default: throw error(format("codec_type %s not supported. Must be audio or video.", codecCtx->codec_type));
	}

	//find an appropriate decode
	auto codec = avcodec_find_decoder(codecCtx->codec_id);
	if (!codec)
		throw error(format("Decoder not found for codecID(%s).", codecCtx->codec_id));

	//force single threaded as h264 probing seems to miss SPS/PPS and seek fails silently
	ffpp::Dict dict(typeid(*this).name(), "decoder", "-threads auto -err_detect 1 -flags output_corrupt -flags2 showall");
	if (avcodec_open2(codecCtx, codec, &dict) < 0)
		throw error("Couldn't open stream.");
	codecCtx->refcounted_frames = true;

	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: {
		auto input = addInput(new Input<DataAVPacket>(this));
		input->setMetadata(new MetadataPktLibavVideo(codecCtx));
		if (codecCtx->codec->capabilities & CODEC_CAP_DR1) {
			codecCtx->thread_safe_callbacks = 1;
			codecCtx->opaque = safe_cast<LibavDirectRendering>(this);
			codecCtx->get_buffer2 = avGetBuffer2;
			videoOutput = addOutputDyn<OutputPicture>(new MetadataRawVideo);
		} else {
			videoOutput = addOutput<OutputPicture>(new MetadataRawVideo);
		}
		break;
	}
	case AVMEDIA_TYPE_AUDIO: {
		auto input = addInput(new Input<DataAVPacket>(this));
		input->setMetadata(new MetadataPktLibavAudio(codecCtx));
		audioOutput = addOutput<OutputPcm>(new MetadataRawAudio);
		break;
	}
	default:
		throw error("Invalid output type.");
	}
}

LibavDecode::~LibavDecode() {
	avcodec_close(codecCtx);
	auto codecCtxCopy = codecCtx;
	avcodec_free_context(&codecCtxCopy);
}

bool LibavDecode::processAudio(const DataAVPacket *data) {
	AVPacket *pkt = data->getPacket();
	int gotFrame = 0;
	if (avcodec_decode_audio4(codecCtx, avFrame->get(), &gotFrame, pkt) < 0) {
		log(Warning, "Error encountered while decoding audio.");
		return false;
	}
	if (av_frame_get_decode_error_flags(avFrame->get()) || (avFrame->get()->flags & AV_FRAME_FLAG_CORRUPT)) {
		log(Error, "Corrupted audio frame decoded.");
	}
	if (gotFrame) {
		auto out = audioOutput->getBuffer(0);
		PcmFormat pcmFormat;
		libavFrame2pcmConvert(avFrame->get(), &pcmFormat);
		out->setFormat(pcmFormat);
		for (uint8_t i = 0; i < pcmFormat.numPlanes; ++i) {
			out->setPlane(i, avFrame->get()->data[i], avFrame->get()->nb_samples * pcmFormat.getBytesPerSample() / pcmFormat.numPlanes);
		}
		auto const &timebase = safe_cast<const MetadataPktLibavAudio>(getInput(0)->getMetadata())->getAVCodecContext()->time_base;
		out->setTime(avFrame->get()->pkt_dts * timebase.num, timebase.den);
		audioOutput->emit(out);
		av_frame_unref(avFrame->get());
		return true;
	}

	av_frame_unref(avFrame->get());
	return false;
}

bool LibavDecode::processVideo(const DataAVPacket *data) {
	AVPacket *pkt = data->getPacket();
	int gotPicture = 0;
	if (avcodec_decode_video2(codecCtx, avFrame->get(), &gotPicture, pkt) < 0) {
		log(Warning, "Error encountered while decoding video.");
		return false;
	}
	if (av_frame_get_decode_error_flags(avFrame->get()) || (avFrame->get()->flags & AV_FRAME_FLAG_CORRUPT)) {
		log(Error, "Corrupted video frame decoded (%s).", gotPicture);
	}
	if (gotPicture) {
		auto ctx = static_cast<LibavDirectRenderingContext*>(avFrame->get()->opaque);
		if (ctx) {
			ctx->pic->setVisibleResolution(Resolution(codecCtx->width, codecCtx->height));
			auto const &timebase = safe_cast<const MetadataPktLibavVideo>(getInput(0)->getMetadata())->getAVCodecContext()->time_base;
			ctx->pic->setTime(avFrame->get()->pkt_dts * timebase.num, timebase.den);
			videoOutput->emit(ctx->pic);
			delete ctx;
		} else {
			auto const pic = DataPicture::create(videoOutput, Resolution(avFrame->get()->width, avFrame->get()->height), libavPixFmt2PixelFormat((AVPixelFormat)avFrame->get()->format));
			copyToPicture(avFrame->get(), pic.get());
			videoOutput->emit(pic);
		}
		av_frame_unref(avFrame->get());
		return true;
	}

	av_frame_unref(avFrame->get());
	return false;
}

LibavDirectRendering::LibavDirectRenderingContext* LibavDecode::getPicture(const Resolution &res, const Resolution &resInternal, const PixelFormat &format) {
	auto ctx = new LibavDirectRenderingContext;
	ctx->pic = DataPicture::create(videoOutput, res, resInternal, format);
	return ctx;
}

void LibavDecode::process(Data data) {
	auto decoderData = safe_cast<const DataAVPacket>(data);
	inputs[0]->updateMetadata(data);
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		processVideo(decoderData.get());
		break;
	case AVMEDIA_TYPE_AUDIO:
		processAudio(decoderData.get());
		break;
	default:
		assert(0);
		return;
	}
}

void LibavDecode::flush() {
	auto nullPkt = uptr(new DataAVPacket(0));
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		while (processVideo(nullPkt.get())) {}
		break;
	case AVMEDIA_TYPE_AUDIO:
		while (processAudio(nullPkt.get())) {}
		break;
	default:
		assert(0);
		break;
	}
}

}
}
