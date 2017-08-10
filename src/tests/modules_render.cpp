#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/render/sdl_audio.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/in/sound_generator.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

unittest("render: A/V sync, one thread") {
	auto videoGen = uptr(create<In::VideoGenerator>());
	auto videoRender = uptr(create<Render::SDLVideo>());
	ConnectOutputToInput(videoGen->getOutput(0), videoRender->getInput(0));

	auto soundGen = uptr(create<In::SoundGenerator>());
	auto soundRender = uptr(create<Render::SDLAudio>());
	ConnectOutputToInput(soundGen->getOutput(0), soundRender->getInput(0));

	for(int i=0; i < 25*5; ++i) {
		videoGen->process(nullptr);
		soundGen->process(nullptr);
	}
}

unittest("render: dynamic resolution") {
	auto videoRender = uptr(create<Render::SDLVideo>());

	std::shared_ptr<DataBase> pic1 = uptr(new PictureYUV420P(Resolution(128, 64)));
	pic1->setMediaTime(1000);
	videoRender->process(pic1);

	std::shared_ptr<DataBase> pic2 = uptr(new PictureYUV420P(Resolution(64, 256)));
	pic2->setMediaTime(2000);
	videoRender->process(pic2);
}

}
