#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/render/sdl_audio.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/in/sound_generator.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_utils/tools.hpp"
#include "lib_utils/sysclock.hpp"

using namespace Tests;
using namespace Modules;

namespace {

#if SIGNALS_HAS_X11
secondclasstest("render: sound generator") {
	auto soundGen = create<In::SoundGenerator>();
	auto render = create<Render::SDLAudio>(make_shared<SystemClock>(1.0));

	ConnectOutputToInput(soundGen->getOutput(0), render->getInput(0));

	for(int i=0; i < 50; ++i) {
		soundGen->work();
	}
	soundGen->flush();
	render->flush();
}

secondclasstest("render: sound generator, evil samples") {
	auto clock = make_shared<SystemClock>(1.0);
	auto render = create<Render::SDLAudio>(clock);

	PcmFormat fmt {};
	fmt.sampleFormat = S16;
	fmt.sampleRate = 44100;
	fmt.numPlanes = 1;

	auto sample = make_shared<DataPcm>(0);
	sample->setMediaTime(299454611464360LL);
	sample->setFormat(fmt);
	sample->setPlane(0, nullptr, 100);
	render->process(sample);

	clock->sleep(1); // wait for crash
}

secondclasstest("render: A/V sync, one thread") {
	auto videoGen = create<In::VideoGenerator>();
	auto videoRender = create<Render::SDLVideo>();
	ConnectOutputToInput(videoGen->getOutput(0), videoRender->getInput(0));

	auto soundGen = create<In::SoundGenerator>();
	auto soundRender = create<Render::SDLAudio>();
	ConnectOutputToInput(soundGen->getOutput(0), soundRender->getInput(0));

	for(int i=0; i < 25*5; ++i) {
		videoGen->work();
		soundGen->work();
	}
}

secondclasstest("render: dynamic resolution") {
	auto videoRender = create<Render::SDLVideo>();

	auto pic1 = make_shared<PictureYUV420P>(Resolution(128, 64));
	pic1->setMediaTime(1000);
	videoRender->process(pic1);

	auto pic2 = make_shared<PictureYUV420P>(Resolution(64, 256));
	pic2->setMediaTime(2000);
	videoRender->process(pic2);
}
#endif

}
