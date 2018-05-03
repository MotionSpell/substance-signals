#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/decode/libav_decode.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_utils/tools.hpp"
#include <iostream> // std::cerr

extern "C" {
#include "libavcodec/avcodec.h"
}

using namespace Tests;
using namespace Modules;

namespace {
std::unique_ptr<Decode::LibavDecode> createGenericDecoder(enum AVCodecID id) {
	auto context = shptr(avcodec_alloc_context3(avcodec_find_decoder(id)));
	context->time_base.num = 1;
	context->time_base.den = 44100; //needed for FFmpeg >= 3.1
	auto metadata = shptr(new MetadataPktLibav(context));
	auto decode = create<Decode::LibavDecode>(metadata);
	return decode;
}

std::unique_ptr<Decode::LibavDecode> createMp3Decoder() {
	return createGenericDecoder(AV_CODEC_ID_MP3);
}

template<size_t numBytes>
std::shared_ptr<DataBase> createAvPacket(uint8_t const (&bytes)[numBytes]) {
	auto pkt = std::make_shared<DataAVPacket>(numBytes);
	memcpy(pkt->data(), bytes, numBytes);
	return pkt;
}

std::shared_ptr<DataBase> getTestMp3Frame() {
	static const uint8_t mp3_sine_frame[] = {
		0xff, 0xfb, 0x30, 0xc0, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x49, 0x6e, 0x66, 0x6f,
		0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x29,
		0x00, 0x00, 0x19, 0xb6, 0x00, 0x0c, 0x0c, 0x12,
		0x12, 0x18, 0x18, 0x18, 0x1e, 0x1e, 0x24, 0x24,
		0x24, 0x2a, 0x2a, 0x30, 0x30, 0x30, 0x36, 0x36,
		0x3c, 0x3c, 0x43, 0x43, 0x43, 0x49, 0x49, 0x4f,
		0x4f, 0x4f, 0x55, 0x55, 0x5b, 0x5b, 0x5b, 0x61,
		0x61, 0x67, 0x67, 0x67, 0x6d, 0x6d, 0x73, 0x73,
		0x79, 0x79, 0x79, 0x7f, 0x7f, 0x86, 0x86, 0x86,
		0x8c, 0x8c, 0x92, 0x92, 0x92, 0x98, 0x98, 0x9e,
		0x9e, 0xa4, 0xa4, 0xa4, 0xaa, 0xaa, 0xb0, 0xb0,
		0xb0, 0xb6, 0xb6, 0xbc, 0xbc, 0xbc, 0xc3, 0xc3,
		0xc9, 0xc9, 0xc9, 0xcf, 0xcf, 0xd5, 0xd5, 0xdb,
		0xdb, 0xdb, 0xe1, 0xe1, 0xe7, 0xe7, 0xe7, 0xed,
		0xed, 0xf3, 0xf3, 0xf3, 0xf9, 0xf9, 0xff, 0xff,
		0x00, 0x00, 0x00, 0x00
	};

	return createAvPacket(mp3_sine_frame);
}
}

unittest("decode: audio simple") {
	auto decode = createMp3Decoder();
	auto null = create<Out::Null>();
	ConnectOutputToInput(decode->getOutput(0), null->getInput(0));

	auto frame = getTestMp3Frame();
	decode->process(frame);
}

namespace {
std::unique_ptr<Decode::LibavDecode> createVideoDecoder() {
	return createGenericDecoder(AV_CODEC_ID_H264);
}

std::shared_ptr<DataBase> getTestH264Frame() {
	static const uint8_t h264_gray_frame[] = {
		0x00, 0x00, 0x00, 0x01,
		0x67, 0x4d, 0x40, 0x0a, 0xe8, 0x8f, 0x42, 0x00,
		0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00,
		0x64, 0x1e, 0x24, 0x4a, 0x24,
		0x00, 0x00, 0x00, 0x01, 0x68, 0xeb, 0xc3, 0xcb,
		0x20, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00,
		0xaf, 0xfd, 0x0f, 0xdf,
	};

	return createAvPacket(h264_gray_frame);
}
}

unittest("decode: video simple") {
	auto decode = createVideoDecoder();
	auto data = getTestH264Frame();

	std::vector<std::string> actualFrames;

	auto onPic = [&](Data data) {
		auto const pic = safe_cast<const DataPicture>(data);
		auto const format = pic->getFormat();
		auto const firstPixel = *pic->getPlane(0);
		auto const lastPixel = *(pic->getPlane(0) + pic->getPitch(0) * (format.res.height-1) + format.res.width - 1);

		char info[256];
		sprintf(info, "'%dx%d %d %.2X %.2X'", format.res.width, format.res.height, format.format, firstPixel, lastPixel);
		actualFrames.push_back(info);
	};

	auto expectedFrames = std::vector<std::string>({
		"'16x16 1 80 80'",
		"'16x16 1 80 80'",
	});

	Connect(decode->getOutput(0)->getSignal(), onPic);
	decode->process(data);
	decode->process(data);
	decode->flush();
	ASSERT_EQUALS(expectedFrames, actualFrames);
}

unittest("decode: audio mp3 manual frame to AAC") {
	auto decode = createMp3Decoder();
	auto encoder = create<Encode::LibavEncode>(Encode::LibavEncode::Audio);

	ConnectOutputToInput(decode->getOutput(0), encoder->getInput(0));

	auto frame = getTestMp3Frame();
	bool thrown = false;
	try {
		decode->process(frame);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}

unittest("decode: audio mp3 to converter to AAC") {
	auto decoder = createMp3Decoder();
	auto encoder = create<Encode::LibavEncode>(Encode::LibavEncode::Audio);

	auto const srcFormat = PcmFormat(44100, 1, AudioLayout::Mono, AudioSampleFormat::S16, AudioStruct::Planar);
	auto const dstFormat = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::F32, AudioStruct::Planar);
	auto const metadataEncoder = safe_cast<const MetadataPktLibav>(encoder->getOutput(0)->getMetadata());
	auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
	auto converter = create<Transform::AudioConvert>(srcFormat, dstFormat, metaEnc->getFrameSize());

	ConnectOutputToInput(decoder->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), encoder->getInput(0));

	auto frame = getTestMp3Frame();
	decoder->process(frame);
	decoder->flush();
	converter->flush();
	encoder->flush();
}
