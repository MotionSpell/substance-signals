#include "tests/tests.hpp"
#include "lib_media/demux/gpac_demux_mp4_simple.hpp"
#include "../gpac_filters.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"


using namespace Tests;
using namespace Modules;

unittest("GpacFilters: audio reframer") {
	Mp4DemuxConfig mp4DemuxCfg;
	mp4DemuxCfg.path = "data/beepbop.mp4";
	mp4DemuxCfg.trackId = 1;
	auto mp4Demux = loadModule("GPACDemuxMP4Simple", &NullHost, &mp4DemuxCfg);

	GpacFiltersConfig gpacFiltersCfg;
	gpacFiltersCfg.filterName = "reframer";
	auto reframer = loadModule("GpacFilters", &NullHost, &gpacFiltersCfg);
	ConnectOutputToInput(mp4Demux->getOutput(0), reframer->getInput(0));

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};
	ConnectOutput(mp4Demux->getOutput(0), onSample);

	for(int i=0; i < 100; ++i)
		mp4Demux->process();

	ASSERT_EQUALS(100, sampleCount);
}

unittest("GpacFilters: video reframer") {
	Mp4DemuxConfig mp4DemuxCfg;
	mp4DemuxCfg.path = "data/beepbop.mp4";
	mp4DemuxCfg.trackId = 2;
	auto mp4Demux = loadModule("GPACDemuxMP4Simple", &NullHost, &mp4DemuxCfg);

	GpacFiltersConfig gpacFiltersCfg;
	gpacFiltersCfg.filterName = "reframer";
	auto reframer = loadModule("GpacFilters", &NullHost, &gpacFiltersCfg);
	ConnectOutputToInput(mp4Demux->getOutput(0), reframer->getInput(0));

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};
	ConnectOutput(mp4Demux->getOutput(0), onSample);

	for(int i=0; i < 100; ++i)
		mp4Demux->process();

	ASSERT_EQUALS(100, sampleCount);
}
