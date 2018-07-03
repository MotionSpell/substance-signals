#include "pipeliner_player.hpp"
#include "lib_pipeline/pipeline.hpp"

// modules
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/render/sdl_audio.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/decode/decoder.hpp"

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
			return pipeline.addModule<Render::SDLVideo>();
		} else if (codecType == AUDIO_RAW) {
			Log::msg(Info, "Found audio stream");
			return pipeline.addModule<Render::SDLAudio>();
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
		return pipeline.addModule<Demux::LibavDemux>(url);
	}
}

}

void declarePipeline(Config cfg, Pipeline &pipeline, const char *url) {
	auto demuxer = createDemuxer(pipeline, url);

	if(demuxer->getNumOutputs() == 0)
		throw std::runtime_error("No streams found");

	for (int k = 0; k < (int)demuxer->getNumOutputs(); ++k) {
		auto metadata = demuxer->getOutput(k)->getMetadata();
		if(!metadata) {
			Log::msg(Debug, "Ignoring stream #%s (no metadata)", k);
			continue;
		}
		if (metadata->isSubtitle()/*only render audio and video*/) {
			Log::msg(Debug, "Ignoring stream #%s", k);
			continue;
		}

		IPipelinedModule* avSource = demuxer;
		int avPin = k;

		if(metadata->getStreamType() != VIDEO_RAW) {
			auto decode = pipeline.addModule<Decode::Decoder>(metadata->getStreamType());
			pipeline.connect(demuxer, k, decode, 0);
			avSource = decode;
			avPin = 0;
		}

		metadata = avSource->getOutput(avPin)->getMetadata();

		auto render = createRenderer(pipeline, cfg, metadata->getStreamType());
		pipeline.connect(avSource, avPin, render, 0);
	}
}
