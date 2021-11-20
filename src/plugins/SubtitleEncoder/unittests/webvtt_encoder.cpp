
#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/common/subtitle.hpp"
#include "../subtitle_encoder.hpp"

using namespace Tests;
using namespace Modules;

namespace {
struct OutStub : ModuleS {
	std::vector<int64_t> times;
	std::vector<std::string> webvtt;
	void processOne(Data data) override {
		webvtt.push_back(std::string((char*)data->data().ptr, data->data().len));
		times.push_back(data->get<PresentationTime>().time);
	}
};
}

unittest("webvtt_encoder") {
	SubtitleEncoderConfig cfg;
	cfg.isWebVTT = true;
	cfg.splitDurationInMs = 1000;
	cfg.maxDelayBeforeEmptyInMs = 2000;
	cfg.timingPolicy = SubtitleEncoderConfig::RelativeToMedia;
	auto m = loadModule("SubtitleEncoder", &NullHost, &cfg);

	Page page {0, IClock::Rate * 4, std::vector<Page::Line>({{"toto"}, {"titi", "#ff0000"}})};
	auto data = std::make_shared<DataSubtitle>(0);
	auto const time = page.showTimestamp + IClock::Rate * 4;
	data->set(DecodingTime{ time });
	data->setMediaTime(time);
	data->page = page;

	auto webvttAnalyzer = createModule<OutStub>();
	ConnectOutputToInput(m->getOutput(0), webvttAnalyzer->getInput(0));

	m->getInput(0)->push(data);

	std::vector<int64_t> expectedTimes = {0, timescaleToClock(cfg.splitDurationInMs, 1000)};
	std::vector<std::string> expectedWebVTT = { R"|(WEBVTT
X-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:0

00:00:00.000 --> 00:00:01.000
toto
titi

)|", R"|(WEBVTT
X-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:0

00:00:01.000 --> 00:00:02.000
toto
titi

)|"};

	ASSERT_EQUALS(expectedTimes, webvttAnalyzer->times);
	ASSERT_EQUALS(expectedWebVTT, webvttAnalyzer->webvtt);
}
