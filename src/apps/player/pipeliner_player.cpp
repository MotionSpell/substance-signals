#include "lib_pipeline/pipeline.hpp"

// modules
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/render/sdl_audio.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/decode/decoder.hpp"

using namespace Modules;
using namespace Pipelines;
using namespace Demux;

static
bool startsWith(std::string s, std::string prefix) {
	return s.substr(0, prefix.size()) == prefix;
}

void declarePipeline(Pipeline &pipeline, const char *url) {
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

	auto createDemuxer = [&](std::string url) {
		if(startsWith(url, "http://")) {
			return pipeline.addModule<DashDemuxer>(url);
		} else {
			return pipeline.addModule<Demux::LibavDemux>(url);
		}
	};

	auto demuxer = createDemuxer(url);

	assert(demuxer->getNumOutputs() > 0);
	for (int k = 0; k < (int)demuxer->getNumOutputs(); ++k) {
		auto metadata = safe_cast<const MetadataPkt>(demuxer->getOutput(k)->getMetadata());
		if (!metadata || metadata->isSubtitle()/*only render audio and video*/) {
			Log::msg(Debug, "Ignoring stream #%s", k);
			continue;
		}

		auto decode = pipeline.addModule<Decode::Decoder>(metadata->getStreamType());
		pipeline.connect(demuxer, k, decode, 0);

		auto render = createRenderer(metadata->getStreamType());
		if (!render)
			continue;

		pipeline.connect(decode, 0, render, 0);
	}
}
