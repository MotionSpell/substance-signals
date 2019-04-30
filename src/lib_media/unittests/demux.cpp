#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/demux/gpac_demux_mp4_simple.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/out/print.hpp"
#include "lib_utils/tools.hpp"
#include "lib_utils/string_tools.hpp"
#include <iostream> // std::cout

using namespace Tests;
using namespace Modules;
using namespace std;

namespace {
vector<int64_t> deltas(vector<int64_t> times) {
	vector<int64_t> r;
	for(int i=0; i < (int)times.size()-1; ++i)
		r.push_back(times[i+1] - times[i]);
	return r;
}
}

// at the moment, the demuxer discards the first frame
unittest("LibavDemux: simple: 75 frames") {

	struct MyOutput : ModuleS {
		void processOne(Data data) override {
			if(isDeclaration(data))
				return;
			++frameCount;
		}
		int frameCount = 0;
	};

	DemuxConfig cfg;
	cfg.url = "data/simple.ts";
	auto demux = loadModule("LibavDemux", &NullHost, &cfg);
	auto rec = createModule<MyOutput>();
	ConnectOutputToInput(demux->getOutput(0), rec->getInput(0));

	for(int i=0; i < 100; ++i)
		demux->process();
	demux->flush();

	ASSERT_EQUALS(75, rec->frameCount);
}

unittest("LibavDemux: rollover") {

	struct MyOutput : ModuleS {
		vector<int64_t> times, decodingTimes;
		void processOne(Data data) override {
			if(isDeclaration(data))
				return;
			times.push_back(data->get<PresentationTime>().time);
			decodingTimes.push_back(data->get<DecodingTime>().time);
		}
	};

	DemuxConfig cfg;
	cfg.url = "data/rollover.ts";
	auto demux = loadModule("LibavDemux", &NullHost, &cfg);
	auto rec = createModule<MyOutput>();
	ConnectOutputToInput(demux->getOutput(0), rec->getInput(0));

	for(int i=0; i < 100; ++i)
		demux->process();
	demux->flush();

	vector<int64_t> expected(74, 7200);
	ASSERT_EQUALS(expected, deltas(rec->times));
	ASSERT_EQUALS(expected, deltas(rec->decodingTimes));
}

unittest("empty param test: Demux") {
	Mp4DemuxConfig cfg {};
	ASSERT_THROWN(loadModule("GPACDemuxMP4Simple", &NullHost, &cfg));
}

secondclasstest("demux one track: Demux::GPACDemuxMP4Simple -> Out::Print") {
	Mp4DemuxConfig cfg { "data/beepbop.mp4"};
	auto mp4Demux = loadModule("GPACDemuxMP4Simple", &NullHost, &cfg);
	auto p = createModule<Out::Print>(&NullHost, std::cout);

	ConnectOutputToInput(mp4Demux->getOutput(0), p->getInput(0));

	for(int i=0; i < 100; ++i)
		mp4Demux->process();
}

unittest("GPACDemuxMP4Full: simple demux one track") {
	auto f = createModule<In::File>(&NullHost, "data/beepbop.mp4");
	auto mp4Demux = loadModule("GPACDemuxMP4Full", &NullHost, nullptr);

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};

	ConnectOutputToInput(f->getOutput(0), mp4Demux->getInput(0));
	ConnectOutput(mp4Demux->getOutput(0), onSample);

	for(int i=0; i < 100; ++i)
		f->process();

	ASSERT_EQUALS(215, sampleCount);
}

unittest("GPACDemuxMP4Full: simple demux one empty track") {
	auto f = createModule<In::File>(&NullHost, "data/emptytrack.mp4");
	auto mp4Demux = loadModule("GPACDemuxMP4Full", &NullHost, nullptr);

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};

	ConnectOutputToInput(f->getOutput(0), mp4Demux->getInput(0));
	ConnectOutput(mp4Demux->getOutput(0), onSample);

	f->process();

	ASSERT_EQUALS(0, sampleCount);

	auto meta = safe_cast<const MetadataPkt>(mp4Demux->getOutput(0)->getMetadata());
	ASSERT_EQUALS("0142C028FFE1001A6742C028116401E0089F961000000300100000030320F1832480010006681020B8CB20",
	    string2hex(meta->codecSpecificInfo.data(), meta->codecSpecificInfo.size()));
}

unittest("GPACDemuxMP4Full: demux fragments") {
	auto f = createModule<In::File>(&NullHost, "data/fragments.mp4");
	auto mp4Demux = loadModule("GPACDemuxMP4Full", &NullHost, nullptr);

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};

	ConnectOutputToInput(f->getOutput(0), mp4Demux->getInput(0));
	ConnectOutput(mp4Demux->getOutput(0), onSample);

	f->process();

	ASSERT_EQUALS(820, sampleCount);
}

