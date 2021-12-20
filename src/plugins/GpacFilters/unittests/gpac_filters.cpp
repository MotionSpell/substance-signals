#include "tests/tests.hpp"
#include "lib_media/demux/gpac_demux_mp4_simple.hpp"
#include "lib_media/in/file.hpp"
#include "plugins/TsDemuxer/ts_demuxer.hpp"
#include "../gpac_filters.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"


using namespace Tests;
using namespace Modules;

unittest("GpacFilters: audio reframer (aac_adts)") {
	FileInputConfig fileInputConfig;
	fileInputConfig.filename = "data/beepbop.ts";
	auto f = loadModule("FileInput", &NullHost, &fileInputConfig);

	TsDemuxerConfig cfg;
	cfg.pids = { TsDemuxerConfig::ANY_AUDIO() };
	auto demux = loadModule("TsDemuxer", &NullHost, &cfg);
	ConnectOutputToInput(f->getOutput(0), demux->getInput(0));

	GpacFiltersConfig gpacFiltersCfg;
	gpacFiltersCfg.filterName = "reframer";
	auto reframer = loadModule("GpacFilters", &NullHost, &gpacFiltersCfg);
	ConnectOutputToInput(demux->getOutput(0), reframer->getInput(0));

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};
	ConnectOutput(reframer->getOutput(0), onSample);

	for(int i=0; i < 100; ++i)
		f->process();

	ASSERT_EQUALS(214, sampleCount);
}

unittest("GpacFilters: video reframer (avc, ts)") {
	FileInputConfig fileInputConfig;
	fileInputConfig.filename = "data/h264.ts";
	auto f = loadModule("FileInput", &NullHost, &fileInputConfig);

	TsDemuxerConfig cfg;
	cfg.pids = { TsDemuxerConfig::ANY_VIDEO() };
	auto demux = loadModule("TsDemuxer", &NullHost, &cfg);
	ConnectOutputToInput(f->getOutput(0), demux->getInput(0));

	GpacFiltersConfig gpacFiltersCfg;
	gpacFiltersCfg.filterName = "reframer";
	auto reframer = loadModule("GpacFilters", &NullHost, &gpacFiltersCfg);
	ConnectOutputToInput(demux->getOutput(0), reframer->getInput(0));

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};
	ConnectOutput(reframer->getOutput(0), onSample);

	for(int i=0; i < 100; ++i)
		f->process();

	ASSERT_EQUALS(71, sampleCount);
}

unittest("GpacFilters: video reframer (m2v, ts)") {
	FileInputConfig fileInputConfig;
	fileInputConfig.filename = "data/simple.ts";
	auto f = loadModule("FileInput", &NullHost, &fileInputConfig);

	TsDemuxerConfig cfg;
	cfg.pids = { TsDemuxerConfig::ANY_VIDEO() };
	auto demux = loadModule("TsDemuxer", &NullHost, &cfg);
	ConnectOutputToInput(f->getOutput(0), demux->getInput(0));

	GpacFiltersConfig gpacFiltersCfg;
	gpacFiltersCfg.filterName = "reframer";
	auto reframer = loadModule("GpacFilters", &NullHost, &gpacFiltersCfg);
	ConnectOutputToInput(demux->getOutput(0), reframer->getInput(0));

	int sampleCount = 0;
	auto onSample = [&](Data) {
		++sampleCount;
	};
	ConnectOutput(reframer->getOutput(0), onSample);

	for(int i=0; i < 100; ++i)
		f->process();

	ASSERT_EQUALS(73, sampleCount);
}

unittest("GpacFilters: video reframer (avc, mp4)") {
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
