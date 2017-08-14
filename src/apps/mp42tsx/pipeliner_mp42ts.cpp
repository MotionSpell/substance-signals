#include "lib_media/media.hpp"
#include "pipeliner.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;

void declarePipeline(Pipeline &pipeline, const mp42tsXOptions &opt) {
	auto connect = [&](auto src, auto dst) {
		pipeline.connect(src, 0, dst, 0);
	};

	auto createSink = [&](bool isHLS)->IPipelinedModule* {
		if (isHLS) {
			const bool isLive = false; //TODO
			const uint64_t segmentDurationInMs = 10000; //TODO
			return pipeline.addModule<Stream::Apple_HLS>("mp42tsx.m3u8", isLive ? Modules::Stream::Apple_HLS::Live : Modules::Stream::Apple_HLS::Static, segmentDurationInMs);
		} else {
			return pipeline.addModule<Out::File>("output.ts"); //FIXME: hardcoded
		}
	};

	const bool isHLS = false; //TODO

	auto demux = pipeline.addModule<Demux::LibavDemux>(opt.url);
	auto m2tsmux = pipeline.addModule<Mux::GPACMuxMPEG2TS>();
	auto sink = createSink(isHLS);
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		pipeline.connect(demux, i, m2tsmux, i);
	}

	connect(m2tsmux, sink);
}
