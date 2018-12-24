#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/in/sound_generator.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

unittest("video generator") {
	auto videoGen = create<In::VideoGenerator>(&NullHost);

	std::vector<int> times;
	auto onFrame = [&](Data data) {
		auto rawData = safe_cast<const DataPicture>(data);
		times.push_back((int)rawData->getMediaTime());
	};
	ConnectOutput(videoGen.get(), onFrame);

	for (int i = 0; i < 4; ++i) {
		videoGen->process();
	}

	ASSERT_EQUALS(makeVector({
		0 * 7200,
		1 * 7200,
		2 * 7200,
		3 * 7200,
	}), times);
}

