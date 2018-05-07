#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_utils/tools.hpp"
#include "lib_media/decode/libav_decode.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/render/sdl_audio.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

secondclasstest("packet type erasure + multi-output: libav Demux -> libav Decoder (Video Only) -> Render::SDL2") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto null = create<Out::Null>();

	size_t videoIndex = std::numeric_limits<size_t>::max();
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		if (demux->getOutput(i)->getMetadata()->isVideo()) {
			videoIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(videoIndex != std::numeric_limits<size_t>::max());
	auto metadata = safe_cast<const MetadataPktLibav>(demux->getOutput(videoIndex)->getMetadata());
	auto decode = create<Decode::LibavDecode>(metadata);
	auto render = create<Render::SDLVideo>();

	ConnectOutputToInput(demux->getOutput(videoIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), render->getInput(0));

	demux->process(nullptr);
}

secondclasstest("packet type erasure + multi-output: libav Demux -> libav Decoder (Audio Only) -> Render::SDL2") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto null = create<Out::Null>();

	size_t audioIndex = std::numeric_limits<size_t>::max();
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		if (demux->getOutput(i)->getMetadata()->getStreamType() == AUDIO_PKT) {
			audioIndex = i;
		} else {
			ConnectOutputToInput(demux->getOutput(i), null->getInput(0));
		}
	}
	ASSERT(audioIndex != std::numeric_limits<size_t>::max());
	auto metadata = safe_cast<const MetadataPktLibav>(demux->getOutput(audioIndex)->getMetadata());
	auto decode = create<Decode::LibavDecode>(metadata);
	auto srcFormat = PcmFormat(44100, 1, AudioLayout::Mono, AudioSampleFormat::F32, AudioStruct::Planar);
	auto dstFormat = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto converter = create<Transform::AudioConvert>(srcFormat, dstFormat);
	auto render = create<Render::SDLAudio>();

	ConnectOutputToInput(demux->getOutput(audioIndex), decode->getInput(0));
	ConnectOutputToInput(decode->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), render->getInput(0));

	demux->process(nullptr);
}

}
