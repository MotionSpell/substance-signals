#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_utils/tools.hpp"
#include "lib_media/decode/decoder.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/out/null.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

#if SIGNALS_HAS_X11
secondclasstest("packet type erasure + multi-output: libav Demux -> libav Decoder (Video Only) -> Render::SDL2") {
	auto demux = create<Demux::LibavDemux>(&NullHost, "data/beepbop.mp4");
	auto null = create<Out::Null>();

	int videoIndex = -1;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		if (demux->getOutput(i)->getMetadata()->isVideo()) {
			videoIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(videoIndex != -1);
	auto metadata = demux->getOutput(videoIndex)->getMetadata();
	auto decode = create<Decode::Decoder>(metadata->type);
	auto render = Modules::loadModule("SDLVideo", &NullHost, nullptr);

	ConnectOutputToInput(demux->getOutput(videoIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), render->getInput(0));

	demux->process();
}

secondclasstest("packet type erasure + multi-output: libav Demux -> libav Decoder (Audio Only) -> Render::SDL2") {
	auto demux = create<Demux::LibavDemux>(&NullHost, "data/beepbop.mp4");
	auto null = create<Out::Null>();

	int audioIndex = -1;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		if (demux->getOutput(i)->getMetadata()->type == AUDIO_PKT) {
			audioIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(audioIndex != -1);
	auto metadata = demux->getOutput(audioIndex)->getMetadata();
	auto decode = create<Decode::Decoder>(metadata->type);
	auto srcFormat = PcmFormat(44100, 1, AudioLayout::Mono, AudioSampleFormat::F32, AudioStruct::Planar);
	auto dstFormat = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto converter = Modules::loadModule("AudioConvert", &NullHost, &srcFormat, &dstFormat, -1);
	auto render = Modules::loadModule("SDLAudio", &NullHost, nullptr);

	ConnectOutputToInput(demux->getOutput(audioIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), render->getInput(0));

	demux->process();
}
#endif

}
