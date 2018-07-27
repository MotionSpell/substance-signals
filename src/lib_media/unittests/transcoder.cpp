#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/decode/jpegturbo_decode.hpp"
#include "lib_media/decode/decoder.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/jpegturbo_encode.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/out/file.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/video_convert.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

void libav_mux(std::string format) {
	auto demux = create<Demux::LibavDemux>(&NullHost, "data/beepbop.mp4");
	auto null = create<Out::Null>();

	//find video signal from demux
	int videoIndex = -1;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = demux->getOutput(i)->getMetadata();
		if (metadata->getStreamType() == VIDEO_PKT) {
			videoIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(videoIndex != -1);

	//create the video decode
	auto metadata = safe_cast<const MetadataPkt>(demux->getOutput(videoIndex)->getMetadata());
	auto decode = create<Decode::Decoder>(VIDEO_PKT);
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video);
	auto mux = create<Mux::LibavMux>("out/output_video_libav", format);

	ConnectOutputToInput(demux->getOutput(videoIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), encode->getInput(0));
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));

	demux->process();
}

unittest("transcoder: video simple (libav mux MP4)") {
	libav_mux("mp4");
}

unittest("transcoder: video simple (libav mux TS)") {
	libav_mux("mpegts");
}

unittest("transcoder: video simple (gpac mux MP4)") {
	auto demux = create<Demux::LibavDemux>(&NullHost, "data/beepbop.mp4");

	//create stub output (for unused demuxer's outputs)
	auto null = create<Out::Null>();

	//find video signal from demux
	int videoIndex = -1;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		if (demux->getOutput(i)->getMetadata()->getStreamType() == VIDEO_PKT) {
			videoIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(videoIndex != -1);

	//create the video decode
	auto decode = create<Decode::Decoder>(VIDEO_PKT);
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video);
	auto mux = create<Mux::GPACMuxMP4>(&NullHost, Mp4MuxConfig{"out/output_video_gpac"});

	ConnectOutputToInput(demux->getOutput(videoIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), encode->getInput(0));
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));

	demux->process();
}

unittest("transcoder: jpg to jpg") {
	const std::string filename("data/sample.jpg");
	auto decode = create<Decode::JPEGTurboDecode>();
	{
		auto preReader = create<In::File>(filename);
		ConnectOutputToInput(preReader->getOutput(0), decode->getInput(0));
		preReader->process();
	}

	auto reader = create<In::File>(filename);
	auto encoder = create<Encode::JPEGTurboEncode>();
	auto writer = create<Out::File>("out/test2.jpg");

	ConnectOutputToInput(reader->getOutput(0), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), encoder->getInput(0));
	ConnectOutputToInput(encoder->getOutput(0), writer->getInput(0));

	reader->process();
}

void resizeJPGTest(PixelFormat pf) {
	const std::string filename("data/sample.jpg");
	auto decode = create<Decode::JPEGTurboDecode>();
	{
		auto preReader = create<In::File>(filename);
		ConnectOutputToInput(preReader->getOutput(0), decode->getInput(0));
		preReader->process();
	}
	auto reader = create<In::File>(filename);

	auto const dstFormat = PictureFormat(Resolution(320, 180) / 2, pf);
	auto converter = create<Transform::VideoConvert>(dstFormat);
	auto encoder = create<Encode::JPEGTurboEncode>();
	auto writer = create<Out::File>("out/test1.jpg");

	ConnectOutputToInput(reader->getOutput(0), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), encoder->getInput(0));
	ConnectOutputToInput(encoder->getOutput(0), writer->getInput(0));

	reader->process();
}

unittest("transcoder: jpg to resized jpg (RGB24)") {
	resizeJPGTest(RGB24);
}

unittest("transcoder: jpg to resized jpg (YUV420)") {
	resizeJPGTest(YUV420P);
}

unittest("transcoder: h264/mp4 to jpg") {
	auto demux = create<Demux::LibavDemux>(&NullHost, "data/beepbop.mp4");

	auto metadata = safe_cast<const MetadataPktLibavVideo>(demux->getOutput(1)->getMetadata());
	auto decode = create<Decode::Decoder>(VIDEO_PKT);

	auto encoder = create<Encode::JPEGTurboEncode>();
	auto writer = create<Out::File>("out/test3.jpg");

	auto const dstRes = metadata->getResolution();
	ASSERT(metadata->getPixelFormat() == YUV420P);
	auto const dstFormat = PictureFormat(dstRes, RGB24);
	auto converter = create<Transform::VideoConvert>(dstFormat);

	ConnectOutputToInput(demux->getOutput(1), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), encoder->getInput(0));
	ConnectOutputToInput(encoder->getOutput(0), writer->getInput(0));

	demux->process();
}

unittest("transcoder: jpg to h264/mp4 (gpac)") {
	const std::string filename("data/sample.jpg");
	auto decode = create<Decode::JPEGTurboDecode>();
	{
		auto preReader = create<In::File>(filename);
		ConnectOutputToInput(preReader->getOutput(0), decode->getInput(0));
		preReader->process();
	}
	auto reader = create<In::File>(filename);

	auto const dstFormat = PictureFormat(Resolution(320, 180), YUV420P);
	auto converter = create<Transform::VideoConvert>(dstFormat);

	auto encoder = create<Encode::LibavEncode>(Encode::LibavEncode::Video);
	auto mux = create<Mux::GPACMuxMP4>(&NullHost, Mp4MuxConfig{"out/test"});

	ConnectOutputToInput(reader->getOutput(0), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), encoder->getInput(0));
	ConnectOutputToInput(encoder->getOutput(0), mux->getInput(0));

	reader->process();
	converter->flush();
	encoder->flush();
	mux->flush();
}

}
