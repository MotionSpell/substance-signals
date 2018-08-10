#include "pipeliner_player.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_modules/utils/loader.hpp"

// modules
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/out/null.hpp"

using namespace Modules;
using namespace Pipelines;

namespace {

bool startsWith(std::string s, std::string prefix) {
	return s.substr(0, prefix.size()) == prefix;
}

IPipelinedModule* createRenderer(Pipeline& pipeline, Config cfg, int codecType) {
	if(!cfg.noRenderer) {
		if (codecType == VIDEO_RAW) {
			Log::msg(Info, "Found video stream");
			return pipeline.add("SDLVideo", nullptr);
		} else if (codecType == AUDIO_RAW) {
			Log::msg(Info, "Found audio stream");
			return pipeline.add("SDLAudio", nullptr);
		}
	}

	Log::msg(Info, "Found unknown stream");
	return pipeline.addModule<Out::Null>();
}

IPipelinedModule* createDemuxer(Pipeline& pipeline, std::string url) {
	if(startsWith(url, "videogen://")) {
		return pipeline.addModule<In::VideoGenerator>();
	}
	if(startsWith(url, "http://")) {
		return pipeline.addModule<Demux::DashDemuxer>(url);
	} else {
		DemuxConfig cfg;
		cfg.url = url;
		return pipeline.addModuleWithHost<Demux::LibavDemux>(cfg);
	}
}

}

void declarePipeline(Config cfg, Pipeline &pipeline, const char *url) {
	auto demuxer = createDemuxer(pipeline, url);

	if(demuxer->getNumOutputs() == 0)
		throw std::runtime_error("No streams found");

	for (int k = 0; k < (int)demuxer->getNumOutputs(); ++k) {
		auto metadata = demuxer->getOutputMetadata(k);
		if(!metadata) {
			Log::msg(Debug, "Ignoring stream #%s (no metadata)", k);
			continue;
		}
		if (metadata->isSubtitle()/*only render audio and video*/) {
			Log::msg(Debug, "Ignoring stream #%s", k);
			continue;
		}

		auto source = GetOutputPin(demuxer, k);

		if(metadata->type != VIDEO_RAW) {
			auto decode = pipeline.add("Decoder", metadata->type);
			pipeline.connect(source, decode);
			source = GetOutputPin(decode);
		}

		metadata = source.mod->getOutputMetadata(source.index);

		auto render = createRenderer(pipeline, cfg, metadata->type);
		pipeline.connect(source, GetInputPin(render));
	}
}
