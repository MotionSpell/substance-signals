#include "decoder.hpp"
#include "../common/pcm.hpp"
#include "lib_utils/tools.hpp"
#include "lib_ffpp/ffpp.hpp"
#include <cassert>

namespace Modules {
namespace Decode {

Decoder::Decoder(std::shared_ptr<const MetadataPktLibav> metadata)
	: codecCtx(shptr(avcodec_alloc_context3(nullptr))), avFrame(new ffpp::Frame) {
	avcodec_copy_context(codecCtx.get(), metadata->getAVCodecContext().get());

	auto const codec = avcodec_find_decoder(codecCtx->codec_id);
	if (!codec)
		throw error(format("Decoder not found for codecID (%s).", codecCtx->codec_id));
	ffpp::Dict dict(typeid(*this).name(), "-threads auto -err_detect 1 -flags output_corrupt -flags2 showall");
	if (avcodec_open2(codecCtx.get(), codec, &dict) < 0)
		throw error("Couldn't open stream.");
	codecCtx->refcounted_frames = true;

	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: {
		auto input = addInput(new Input<DataAVPacket>(this));
		input->setMetadata(shptr(new MetadataPktLibavVideo(codecCtx)));
		if (codecCtx->codec->capabilities & AV_CODEC_CAP_DR1) {
			codecCtx->thread_safe_callbacks = 1;
			codecCtx->opaque = static_cast<PictureAllocator*>(this);
			codecCtx->get_buffer2 = avGetBuffer2;
			videoOutput = addOutputDynAlloc<OutputPicture>(std::thread::hardware_concurrency() * 4, shptr(new MetadataRawVideo));
		} else {
			videoOutput = addOutput<OutputPicture>(shptr(new MetadataRawVideo));
		}
		deliverOutput = std::bind(&Decoder::processVideo, this);
		break;
	}
	case AVMEDIA_TYPE_AUDIO: {
		auto input = addInput(new Input<DataAVPacket>(this));
		input->setMetadata(shptr(new MetadataPktLibavAudio(codecCtx)));
		audioOutput = addOutput<OutputPcm>(shptr(new MetadataRawAudio));
		deliverOutput = std::bind(&Decoder::processAudio, this);
		break;
	}
	default:
		throw error(format("codec_type %s not supported. Must be audio or video.", codecCtx->codec_type));
	}
}

Decoder::~Decoder() {
}

void Decoder::processAudio() {

	auto out = audioOutput->getBuffer(0);
	PcmFormat pcmFormat;
	libavFrame2pcmConvert(avFrame->get(), &pcmFormat);
	out->setFormat(pcmFormat);
	for (uint8_t i = 0; i < pcmFormat.numPlanes; ++i) {
		out->setPlane(i, avFrame->get()->data[i], avFrame->get()->nb_samples * pcmFormat.getBytesPerSample() / pcmFormat.numPlanes);
	}

	setMediaTime(out.get());

	audioOutput->emit(out);
}

void Decoder::processVideo() {

	std::shared_ptr<DataPicture> pic;
	auto ctx = static_cast<PictureContext*>(avFrame->get()->opaque);
	if (ctx) {
		pic = ctx->pic;
		ctx->pic->setVisibleResolution(Resolution(codecCtx->width, codecCtx->height));
	} else {
		pic = DataPicture::create(videoOutput, Resolution(avFrame->get()->width, avFrame->get()->height), libavPixFmt2PixelFormat((AVPixelFormat)avFrame->get()->format));
		copyToPicture(avFrame->get(), pic.get());
	}

	setMediaTime(pic.get());

	if (videoOutput) videoOutput->emit(pic);
}

void Decoder::setMediaTime(DataBase* data) {
	data->setMediaTime(avFrame->get()->pts);
}

PictureAllocator::PictureContext* Decoder::getPicture(Resolution res, Resolution resInternal, PixelFormat format) {
	auto ctx = new PictureAllocator::PictureContext;
	ctx->pic = DataPicture::create(videoOutput, res, resInternal, format);
	return ctx;
}

void Decoder::process(Data data) {
	inputs[0]->updateMetadata(data);

	AVPacket *pkt = safe_cast<const DataAVPacket>(data)->getPacket();
	if (pkt->flags & AV_PKT_FLAG_RESET_DECODER) {
		avcodec_flush_buffers(codecCtx.get());
		pkt->flags &= ~AV_PKT_FLAG_RESET_DECODER;
	}

	pkt->pts = data->getMediaTime();
	processPacket(pkt);
}

void Decoder::processPacket(AVPacket const * pkt) {
	int ret;

	ret = avcodec_send_packet(codecCtx.get(), pkt);
	if (ret < 0) {
		log(Warning, "Decoding error: %s", avStrError(ret));
		return;
	}

	while(1) {
		ret = avcodec_receive_frame(codecCtx.get(), avFrame->get());
		if(ret != 0)
			break; // no more frames

		if (av_frame_get_decode_error_flags(avFrame->get()) || (avFrame->get()->flags & AV_FRAME_FLAG_CORRUPT)) {
			log(Error, "Corrupted frame decoded");
		}

		deliverOutput();
	}
}

void Decoder::flush() {
	processPacket(nullptr);
}

}
}
