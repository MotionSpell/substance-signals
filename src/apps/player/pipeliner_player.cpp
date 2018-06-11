#include "lib_pipeline/pipeline.hpp"

// modules
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/demux/gpac_demux_mp4_full.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/render/sdl_audio.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/decode/decoder.hpp"

using namespace Modules;
using namespace Transform;
using namespace Pipelines;
using namespace In;
using namespace Demux;

static
bool startsWith(std::string s, std::string prefix) {
	return s.substr(0, prefix.size()) == prefix;
}

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

	std::unique_ptr<IFilePuller> createHttpSource();

	struct OutputDesc {
		IPipelinedModule* module;
		int index;
		std::shared_ptr<const IMetadata> metadata;
	};

	auto createSources = [&](std::string url) -> std::vector<OutputDesc> {
		std::vector<OutputDesc> r;
		if(startsWith(url, "http://")) {
			auto dashInput = pipeline.addModule<MPEG_DASH_Input>(createHttpSource(), url);
			for (int i = 0; i < (int)dashInput->getNumOutputs(); ++i) {
				auto demux = pipeline.addModule<GPACDemuxMP4Full>();
				pipeline.connect(dashInput, i, demux, 0);
				auto restamp = pipeline.addModule<Restamp>(Transform::Restamp::Reset);
				pipeline.connect(demux, 0, restamp, 0);
				r.push_back({restamp, 0, dashInput->getOutput(i)->getMetadata()});
			}
		} else {
			auto demux = pipeline.addModule<Demux::LibavDemux>(url);
			for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
				r.push_back({demux, i, demux->getOutput(i)->getMetadata()});
			}
		}
		return r;
	};

	auto streams = createSources(url);

	for (int k = 0; k < (int)streams.size(); ++k) {
		auto s = streams[k];
		auto metadata = safe_cast<const MetadataPkt>(s.metadata);
		if (!metadata || metadata->isSubtitle()/*only render audio and video*/) {
			Log::msg(Debug, "Ignoring stream #%s", k);
			continue;
		}

		auto decode = pipeline.addModule<Decode::Decoder>(metadata->getStreamType());
		pipeline.connect(s.module, s.index, decode, 0);

		auto render = createRenderer(metadata->getStreamType());
		if (!render)
			continue;

		connect(decode, render);
	}
}
