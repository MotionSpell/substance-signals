#include "pipeliner_player.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_utils/log.hpp" // g_Log
#include "lib_utils/format.hpp"
#include <fstream>

// modules
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/out/null.hpp"
#include "plugins/HlsDemuxer/hls_demux.hpp"
#include "plugins/TsDemuxer/ts_demuxer.hpp"

using namespace Modules;
using namespace Pipelines;

namespace {

struct ZeroRestamper : Module {
		ZeroRestamper(KHost*, int count) {
			for(int i=0; i < count; ++i) {
				addInput();
				addOutput();
			}
		}

		void process() override {
			int idx;
			auto dataIn = popAny(idx);

			auto dataTime = dataIn->get<PresentationTime>().time;

			if(startTime == INT64_MIN)
				startTime = dataTime;

			auto restampedTime = dataTime - startTime;

			auto dataOut = clone(dataIn);
			dataOut->copyAttributes(*dataIn);
			dataOut->set(PresentationTime{restampedTime});

			outputs[idx]->post(dataOut);
		}

	private:
		int64_t startTime = INT64_MIN;

		Data popAny(int& inputIdx) {
			Data data;
			inputIdx = 0;
			while (!inputs[inputIdx]->tryPop(data))
				inputIdx++;
			return data;
		}

};

struct LocalFileSystem : In::IFilePuller {
	void wget(const char* szUrl, std::function<void(SpanC)> callback) {
		printf("LocalFileSystem: wget('%s')\n", szUrl);
		std::ifstream fp(szUrl, std::ios::binary);
		if(!fp.is_open())
			return;

		fp.seekg(0, std::ios::end);
		auto const size = fp.tellg();
		fp.seekg(0, std::ios::beg);

		std::vector<uint8_t> buf(size);
		fp.read((char*)buf.data(), buf.size());

		callback({buf.data(), buf.size()});
	}
};

bool startsWith(std::string s, std::string prefix) {
	return s.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string s, std::string suffix) {
	return s.substr(s.size() - suffix.size(), suffix.size()) == suffix;
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
		g_Log->log(Info, "Found unknown stream");
	}

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
		return pipeline.addModule<In::VideoGenerator>(url.c_str());
	}
	if(endsWith(url, ".m3u8")) {
		HlsDemuxConfig hlsCfg;
		if(!startsWith(url, "http://") && !startsWith(url, "https://") ) {
			static LocalFileSystem fs;
			hlsCfg.filePuller = &fs;
		}
		hlsCfg.url = url;
		auto recv = pipeline.add("HlsDemuxer", &hlsCfg);
		TsDemuxerConfig tsCfg {};
		auto demux = pipeline.add("TsDemuxer", &tsCfg);
		pipeline.connect(recv, demux);
		return demux;
	}
	if(startsWith(url, "http://") || startsWith(url, "https://")) {
		DashDemuxConfig cfg;
		cfg.url = url;
		return pipeline.add("DashDemuxer", &cfg);
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

	auto restamper = pipeline.addModule<ZeroRestamper>(demuxer->getNumOutputs());

	for (int k = 0; k < demuxer->getNumOutputs(); ++k) {
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
			auto decode = pipeline.add("Decoder", (void*)(uintptr_t)metadata->type);
			pipeline.connect(source, decode);
			source = GetOutputPin(decode);
		}

		metadata = source.mod->getOutputMetadata(source.index);

		auto render = createRenderer(pipeline, cfg, metadata->type);
		pipeline.connect(source, GetInputPin(restamper, k));
		pipeline.connect(GetOutputPin(restamper, k), GetInputPin(render));
	}
}
