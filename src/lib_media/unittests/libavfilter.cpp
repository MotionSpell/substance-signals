#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/transform/libavfilter.hpp"
#include <vector>

using namespace Tests;
using namespace Modules;
using namespace std;


unittest("avfilter: deinterlace") {
	vector<int64_t> times;
	auto onFrame = [&](Data data) {
		times.push_back(data->getMediaTime());
	};

	auto videoGen = create<In::VideoGenerator>(&NullHost);
	PictureFormat fmt(Resolution(320, 180), PixelFormat::I420);

	auto cfg = AvFilterConfig { fmt, "yadif=0:-1:0" };
	auto filter = loadModule("LibavFilter", &NullHost, &cfg);

	ConnectOutputToInput(videoGen->getOutput(0), filter->getInput(0));
	ConnectOutput(filter.get(), onFrame);

	for (int i = 0; i < 4; ++i) {
		videoGen->process();
	}
	filter->flush();

	ASSERT_EQUALS(vector<int64_t>({0, 7200, 14400}), times);
}

unittest("avfilter: fps convert (drop/repeat)") {
	vector<int64_t> times;
	auto onFrame = [&](Data data) {
		times.push_back(data->getMediaTime());
	};

	auto videoGen = create<In::VideoGenerator>(&NullHost);
	PictureFormat fmt(Resolution(320, 180), PixelFormat::I420);

	auto cfg = AvFilterConfig { fmt, "fps=30000/1001:0.0" };
	auto filter = loadModule("LibavFilter", &NullHost, &cfg);

	ConnectOutputToInput(videoGen->getOutput(0), filter->getInput(0));
	ConnectOutput(filter.get(), onFrame);

	for (int i = 0; i < 4; ++i) {
		videoGen->process();
	}
	filter->flush();

	ASSERT_EQUALS(vector<int64_t>({0, 6006, 12012, 18018}), times);
}
