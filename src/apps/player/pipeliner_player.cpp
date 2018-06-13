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

// holds the chain: [dash downloader] => ( [mp4demuxer] => [restamper] )*
class DashDemuxer : public Module {
	public:
		DashDemuxer(std::unique_ptr<IFilePuller> fp, std::string url) {
			auto downloader = pipeline.addModule<MPEG_DASH_Input>(std::move(fp), url);

			for (int i = 0; i < (int)downloader->getNumOutputs(); ++i)
				addStream(downloader->getOutput(i));
		}

		virtual void process() override {
			pipeline.start();
			pipeline.waitForEndOfStream();
		}

	private:
		void addStream(IOutput* downloadOutput) {
			auto meta = downloadOutput->getMetadata();

			// create our own output
			auto output = addOutput<OutputDefault>();
			output->setMetadata(meta);

			// add MP4 demuxer
			auto decap = pipeline.addModule<GPACDemuxMP4Full>();
			ConnectOutputToInput(downloadOutput, decap->getInput(0));

			// add restamper (so the timestamps start at zero)
			auto restamp = pipeline.addModule<Restamp>(Transform::Restamp::Reset);
			ConnectOutputToInput(decap->getOutput(0), restamp->getInput(0));

			ConnectOutput(restamp, [output](Data data) {
				output->emit(data);
			});

			auto null = pipeline.addModule<Out::Null>();
			pipeline.connect(restamp, 0, null, 0);
		}

		Pipeline pipeline;
};

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

	std::unique_ptr<IFilePuller> createHttpSource();

	auto createDemuxer = [&](std::string url) {
		if(startsWith(url, "http://")) {
			return pipeline.addModule<DashDemuxer>(createHttpSource(), url);
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
