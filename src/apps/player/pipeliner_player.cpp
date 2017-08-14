#include "lib_media/media.hpp"
#include "lib_appcommon/pipeliner.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;

void declarePipeline(Pipeline &pipeline, const char *url) {
	auto connect = [&](auto src, auto dst) {
		pipeline.connect(src, 0, dst, 0);
	};

	auto createRenderer = [&](int codecType)->IPipelinedModule* {
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "Found video stream");
			return pipeline.addModule<Render::SDLVideo>();
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "Found audio stream");
			return pipeline.addModule<Render::SDLAudio>();
		} else {
			Log::msg(Info, "Found unknown stream");
			return pipeline.addModule<Out::Null>();
		}
	};

	auto demux = pipeline.addModule<Demux::LibavDemux>(url);
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
		auto metadata = safe_cast<const MetadataPktLibav>(demux->getOutput(i)->getMetadata());
		if (!metadata || metadata->isSubtitle()/*only render audio and video*/)
			continue;

		auto decode = pipeline.addModule<Decode::LibavDecode>(metadata);
		pipeline.connect(demux, i, decode, 0);

		auto render = createRenderer(metadata->getStreamType());
		if (!render)
			continue;

		connect(decode, render);
	}
}
