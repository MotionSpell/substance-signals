#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/common/picture.hpp" // PictureFormat
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_media/mux/mux_mp4_config.hpp"
#include "lib_media/out/file.hpp"
#include "lib_media/out/null.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

void libav_mux(std::string format) {
	DemuxConfig cfg;
	cfg.url = "data/beepbop.mp4";
	auto demux = loadModule("LibavDemux", &NullHost, &cfg);
	auto null = create<Out::Null>(&NullHost);

	//find video signal from demux
	int videoIndex = -1;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = demux->getOutput(i)->getMetadata();
		if (metadata->type == VIDEO_PKT) {
			videoIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(videoIndex != -1);

	//create the video decode
	auto metadata = safe_cast<const MetadataPkt>(demux->getOutput(videoIndex)->getMetadata());
	auto decode = loadModule("Decoder", &NullHost, VIDEO_PKT);
	EncoderConfig encCfg { EncoderConfig::Video };
	auto encode = loadModule("Encoder", &NullHost, &encCfg);
	MuxConfig muxConfig = {"out/output_video_libav", format, ""};
	auto mux = loadModule("LibavMux", &NullHost, &muxConfig);

	ConnectOutputToInput(demux->getOutput(videoIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), encode->getInput(0));
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));

	demux->process();
}
}

unittest("transcoder: video simple (libav mux MP4)") {
	libav_mux("mp4");
}

unittest("transcoder: video simple (libav mux TS)") {
	libav_mux("mpegts");
}

unittest("transcoder: video simple (gpac mux MP4)") {
	DemuxConfig cfg;
	cfg.url = "data/beepbop.mp4";
	auto demux = loadModule("LibavDemux", &NullHost, &cfg);

	//create stub output (for unused demuxer's outputs)
	auto null = create<Out::Null>(&NullHost);

	//find video signal from demux
	int videoIndex = -1;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		if (demux->getOutput(i)->getMetadata()->type == VIDEO_PKT) {
			videoIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(videoIndex != -1);

	//create the video decode
	auto decode = loadModule("Decoder", &NullHost, VIDEO_PKT);
	EncoderConfig encCfg { EncoderConfig::Video };
	auto encode = loadModule("Encoder", &NullHost, &encCfg);

	auto muxCfg = Mp4MuxConfig{"out/output_video_gpac"};
	auto mux = loadModule("GPACMuxMP4", &NullHost, &muxCfg);

	ConnectOutputToInput(demux->getOutput(videoIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), encode->getInput(0));
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));

	demux->process();
}

unittest("transcoder: jpg to jpg") {
	const std::string filename("data/sample.jpg");
	auto decode = loadModule("JPEGTurboDecode", &NullHost);
	{
		auto preReader = create<In::File>(&NullHost, filename);
		ConnectOutputToInput(preReader->getOutput(0), decode->getInput(0));
		preReader->process();
	}

	auto reader = create<In::File>(&NullHost, filename);
	auto encoder = loadModule("JPEGTurboEncode", &NullHost);
	auto writer = create<Out::File>(&NullHost, "out/test2.jpg");

	ConnectOutputToInput(reader->getOutput(0), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), encoder->getInput(0));
	ConnectOutputToInput(encoder->getOutput(0), writer->getInput(0));

	reader->process();
}

void resizeJPGTest(PixelFormat pf) {
	const std::string filename("data/sample.jpg");
	auto decode = loadModule("JPEGTurboDecode", &NullHost);
	{
		auto preReader = create<In::File>(&NullHost, filename);
		ConnectOutputToInput(preReader->getOutput(0), decode->getInput(0));
		preReader->process();
	}
	auto reader = create<In::File>(&NullHost, filename);

	auto const dstFormat = PictureFormat(Resolution(320, 180) / 2, pf);
	auto converter = loadModule("VideoConvert", &NullHost, &dstFormat);
	auto encoder = loadModule("JPEGTurboEncode", &NullHost);
	auto writer = create<Out::File>(&NullHost, "out/test1.jpg");

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
	DemuxConfig cfg;
	cfg.url = "data/beepbop.mp4";
	auto demux = loadModule("LibavDemux", &NullHost, &cfg);

	auto metadata = safe_cast<const MetadataPktVideo>(demux->getOutput(1)->getMetadata());
	auto decode = loadModule("Decoder", &NullHost, VIDEO_PKT);

	auto encoder = loadModule("JPEGTurboEncode", &NullHost);
	auto writer = create<Out::File>(&NullHost, "out/test3.jpg");

	auto const dstRes = metadata->resolution;
	ASSERT(metadata->pixelFormat == YUV420P);
	auto const dstFormat = PictureFormat(dstRes, RGB24);
	auto converter = loadModule("VideoConvert", &NullHost, &dstFormat);

	ConnectOutputToInput(demux->getOutput(1), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), encoder->getInput(0));
	ConnectOutputToInput(encoder->getOutput(0), writer->getInput(0));

	demux->process();
}

unittest("transcoder: jpg to h264/mp4 (gpac)") {
	const std::string filename("data/sample.jpg");
	auto decode = loadModule("JPEGTurboDecode", &NullHost);
	{
		auto preReader = create<In::File>(&NullHost, filename);
		ConnectOutputToInput(preReader->getOutput(0), decode->getInput(0));
		preReader->process();
	}
	auto reader = create<In::File>(&NullHost, filename);

	auto const dstFormat = PictureFormat(Resolution(320, 180), YUV420P);
	auto converter = loadModule("VideoConvert", &NullHost, &dstFormat);

	EncoderConfig cfg { EncoderConfig::Video };
	auto encoder = loadModule("Encoder", &NullHost, &cfg);
	auto muxCfg = Mp4MuxConfig{"out/test"};
	auto mux = loadModule("GPACMuxMP4", &NullHost, &muxCfg);

	ConnectOutputToInput(reader->getOutput(0), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), encoder->getInput(0));
	ConnectOutputToInput(encoder->getOutput(0), mux->getInput(0));

	reader->process();
	converter->flush();
	encoder->flush();
	mux->flush();
}

