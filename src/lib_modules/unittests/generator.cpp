#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/in/sound_generator.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

#if SIGNALS_HAS_X11

#include "lib_media/render/sdl_audio.hpp"

secondclasstest("sound generator") {
	auto soundGen = create<In::SoundGenerator>();
	auto render = create<Render::SDLAudio>();

	ConnectOutputToInput(soundGen->getOutput(0), render->getInput(0));

	for(int i=0; i < 25; ++i) {
		soundGen->process(nullptr);
	}
}

#endif

unittest("video generator") {
	auto videoGen = create<In::VideoGenerator>();

	std::vector<int> times;
	auto onFrame = [&](Data data) {
		auto rawData = safe_cast<const DataPicture>(data);
		times.push_back((int)rawData->getMediaTime());
	};
	Connect(videoGen->getOutput(0)->getSignal(), onFrame);

	for (int i = 0; i < 50; ++i) {
		videoGen->process(nullptr);
	}

	ASSERT_EQUALS(makeVector({0, 7200, 180000, 187200}), times);
}

