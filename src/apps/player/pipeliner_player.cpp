#include "pipeliner_player.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_modules/utils/loader.hpp"

// modules
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/demux/TsDemuxer/ts_demuxer.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/out/null.hpp"

using namespace Modules;
using namespace Pipelines;

namespace {

bool startsWith(std::string s, std::string prefix) {
	return s.substr(0, prefix.size()) == prefix;
}

IFilter* createRenderer(Pipeline& pipeline, Config cfg, int codecType) {
	if(!cfg.noRenderer) {
		if (codecType == VIDEO_RAW) {
			g_Log->log(Info, "Found video stream");
			return pipeline.add("SDLVideo", nullptr);
		} else if (codecType == AUDIO_RAW) {
			g_Log->log(Info, "Found audio stream");
			return pipeline.add("SDLAudio", nullptr);
		}
	}

	g_Log->log(Info, "Found unknown stream");
	return pipeline.addModule<Out::Null>();
}

IFilter* createDemuxer(Pipeline& pipeline, std::string url) {
	if(startsWith(url, "mpegts://")) {
		url = url.substr(9);
		auto file = pipeline.addModule<In::File>(url);
		TsDemuxerConfig cfg {};
		auto demux = pipeline.add("TsDemuxer", &cfg);
		pipeline.connect(file, demux);
		return demux;
	}
	if(startsWith(url, "videogen://")) {
		return pipeline.addModule<In::VideoGenerator>();
	}
	if(startsWith(url, "http://")) {
		return pipeline.addModule<Demux::DashDemuxer>(url);
	} else {
		DemuxConfig cfg;
		cfg.url = url;
		return pipeline.add("LibavDemux", &cfg);
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
			g_Log->log(Debug, format("Ignoring stream #%s (no metadata)", k).c_str());
			continue;
		}
		if (metadata->isSubtitle()/*only render audio and video*/) {
			g_Log->log(Debug, format("Ignoring stream #%s", k).c_str());
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
