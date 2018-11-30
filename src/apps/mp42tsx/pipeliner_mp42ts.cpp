#include "pipeliner_mp42ts.hpp"

// modules
#include "lib_media/stream/apple_hls.hpp"
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
		pipeline.connect(GetOutputPin(demux, i), GetInputPin(mux, i));
	}
}
