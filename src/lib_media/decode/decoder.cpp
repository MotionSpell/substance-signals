#include "decoder.hpp"
#include "../common/pcm.hpp"
#include "lib_utils/tools.hpp"
#include "lib_ffpp/ffpp.hpp"
#include <cassert>

namespace Modules {
namespace Decode {

Decoder::Decoder(std::shared_ptr<const MetadataPkt> metadata)
	: avFrame(new ffpp::Frame) {

	auto const codec_id = (AVCodecID)metadata->codecId;
	auto const extradata = metadata->codecSpecificInfo;

	auto const codec = avcodec_find_decoder(codec_id);
	if (!codec)
		throw error(format("Decoder not found for codecID (%s).", codec_id));

	codecCtx = shptr(avcodec_alloc_context3(codec));

	// copy extradata: this allows decoding non-Annex B bitstreams
	// (i.e AVCC / H264-in-mp4).
	{
		codecCtx->extradata = (uint8_t*)av_calloc(1, extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE);
		codecCtx->extradata_size = (int)extradata.size();
		memcpy(codecCtx->extradata, extradata.data(), extradata.size());
	}

	ffpp::Dict dict(typeid(*this).name(), "-threads auto -err_detect 1 -flags output_corrupt -flags2 showall");
	if (avcodec_open2(codecCtx.get(), codec, &dict) < 0)
		throw error("Couldn't open stream.");

	addInput(new Input<DataBase>(this));

	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO: {
		if (codecCtx->codec->capabilities & AV_CODEC_CAP_DR1) {
			codecCtx->thread_safe_callbacks = 1;
			codecCtx->opaque = static_cast<PictureAllocator*>(this);
			codecCtx->get_buffer2 = avGetBuffer2;
			videoOutput = addOutputDynAlloc<OutputPicture>(std::thread::hardware_concurrency() * 4, make_shared<MetadataRawVideo>());
		} else {
			videoOutput = addOutput<OutputPicture>(make_shared<MetadataRawVideo>());
		}
		getDecompressedData = std::bind(&Decoder::processVideo, this);
		break;
	}
	case AVMEDIA_TYPE_AUDIO: {
		audioOutput = addOutput<OutputPcm>(make_shared<MetadataRawAudio>());
		getDecompressedData = std::bind(&Decoder::processAudio, this);
		break;
	}
	default:
		throw error(format("codec_type %s not supported. Must be audio or video.", codecCtx->codec_type));
	}
}

Decoder::~Decoder() {
}

std::shared_ptr<DataBase> Decoder::processAudio() {

	auto out = audioOutput->getBuffer(0);
	PcmFormat pcmFormat;
	libavFrame2pcmConvert(avFrame->get(), &pcmFormat);
	out->setFormat(pcmFormat);
	for (uint8_t i = 0; i < pcmFormat.numPlanes; ++i) {
		out->setPlane(i, avFrame->get()->data[i], avFrame->get()->nb_samples * pcmFormat.getBytesPerSample() / pcmFormat.numPlanes);
	}

	return out;
}

std::shared_ptr<DataBase> Decoder::processVideo() {

	std::shared_ptr<DataPicture> pic;
	if (auto ctx = static_cast<PictureContext*>(avFrame->get()->opaque)) {
		pic = ctx->pic;
		ctx->pic->setVisibleResolution(Resolution(codecCtx->width, codecCtx->height));
	} else {
		pic = DataPicture::create(videoOutput, Resolution(avFrame->get()->width, avFrame->get()->height), libavPixFmt2PixelFormat((AVPixelFormat)avFrame->get()->format));
		copyToPicture(avFrame->get(), pic.get());
	}

	return pic;
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

	if (data->flags & DATA_FLAGS_DISCONTINUITY) {
		avcodec_flush_buffers(codecCtx.get());
	}

	AVPacket pkt {};
	pkt.pts = data->getMediaTime();
	pkt.data = (uint8_t*)data->data();
	pkt.size = (int)data->size();
	processPacket(&pkt);
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

		if (avFrame->get()->decode_error_flags || (avFrame->get()->flags & AV_FRAME_FLAG_CORRUPT)) {
			log(Error, "Corrupted frame decoded");
		}

		auto data = getDecompressedData();
		setMediaTime(data.get());
		outputs[0]->emit(data);
	}
}

void Decoder::flush() {
	processPacket(nullptr);
}

}
}
