#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/render/sdl_audio.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/in/sound_generator.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

#if SIGNALS_HAS_X11
secondclasstest("render: sound generator") {
	auto soundGen = create<In::SoundGenerator>();
	auto render = create<Render::SDLAudio>();

	ConnectOutputToInput(soundGen->getOutput(0), render->getInput(0));

	for(int i=0; i < 25; ++i) {
		soundGen->process(nullptr);
	}
}

secondclasstest("render: A/V sync, one thread") {
	auto videoGen = create<In::VideoGenerator>();
	auto videoRender = create<Render::SDLVideo>();
	ConnectOutputToInput(videoGen->getOutput(0), videoRender->getInput(0));

	auto soundGen = create<In::SoundGenerator>();
	auto soundRender = create<Render::SDLAudio>();
	ConnectOutputToInput(soundGen->getOutput(0), soundRender->getInput(0));

	for(int i=0; i < 25*5; ++i) {
		videoGen->process(nullptr);
		soundGen->process(nullptr);
	}
}

secondclasstest("render: dynamic resolution") {
	auto videoRender = create<Render::SDLVideo>();

	auto pic1 = shptr(new PictureYUV420P(Resolution(128, 64)));
	pic1->setMediaTime(1000);
	videoRender->process(pic1);

	auto pic2 = shptr(new PictureYUV420P(Resolution(64, 256)));
	pic2->setMediaTime(2000);
	videoRender->process(pic2);
}
#endif

}
