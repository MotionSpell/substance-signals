#include "pipeliner_mp42ts.hpp"
#include "lib_utils/system_clock.hpp"

// modules
#include "lib_media/utils/regulator.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/mux/libav_mux.hpp"

using namespace Modules;
using namespace Pipelines;

void declarePipeline(Pipeline &pipeline, const mp42tsXOptions &opt) {
	DemuxConfig cfg;
	cfg.url = opt.url;
	auto demux = pipeline.add("LibavDemux", &cfg);

	MuxConfig muxCfg;
	muxCfg.format = "mpegts";
	muxCfg.path = opt.output;
	auto mux = pipeline.add("LibavMux", &muxCfg);
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto flow = GetOutputPin(demux, i);
		if(opt.isLive) {
			auto regulator = pipeline.addModule<Regulator>(g_SystemClock);
			pipeline.connect(flow, regulator);
			flow = GetOutputPin(regulator);
		}
		pipeline.connect(flow, GetInputPin(mux, i));
	}
}
