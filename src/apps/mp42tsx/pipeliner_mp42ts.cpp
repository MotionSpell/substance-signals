#include "pipeliner_mp42ts.hpp"

// modules
#include "lib_media/stream/apple_hls.hpp"
#include "lib_media/out/file.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/mux/gpac_mux_m2ts.hpp"

using namespace Modules;
using namespace Pipelines;

void declarePipeline(Pipeline &pipeline, const mp42tsXOptions &opt) {
	auto createSink = [&](bool isHLS)->IPipelinedModule* {
		if (isHLS) {
			const bool isLive = false; //TODO
			const uint64_t segmentDurationInMs = 10000; //TODO
			return pipeline.addModule<Stream::Apple_HLS>("", "mp42tsx.m3u8", isLive ? Modules::Stream::Apple_HLS::Live : Modules::Stream::Apple_HLS::Static, segmentDurationInMs);
		} else {
			return pipeline.addModule<Out::File>(opt.output);
		}
	};

	const bool isHLS = false; //TODO

	DemuxConfig cfg;
	cfg.url = opt.url;
	auto demux = pipeline.add("LibavDemux", &cfg);
	auto m2tsmux = pipeline.addModule<Mux::GPACMuxMPEG2TS>(GF_FALSE, 0);
	auto sink = createSink(isHLS);
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		if (demux->getOutputMetadata(i)->isVideo()) { //FIXME
			pipeline.connect(GetOutputPin(demux, i), m2tsmux);
			break; //FIXME
		}
	}

	pipeline.connect(m2tsmux, sink);
}
