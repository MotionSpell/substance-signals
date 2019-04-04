#include "hls_muxer_libav.hpp"

#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/loader.hpp"
#include "../mux/libav_mux.hpp"
#include "../common/libav.hpp"
#include "../common/metadata_file.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/log_sink.hpp"
#include "lib_utils/tools.hpp" // safe_cast
#include <cassert>

extern "C" {
#include <libavcodec/avcodec.h> // AVPacket
}

using namespace Modules;

namespace {

bool fileExists(std::string path) {
	auto f = fopen(path.c_str(), "r");
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

uint64_t fileSize(std::string path) {
	auto file = fopen(path.c_str(), "rt");
	if (!file)
		throw std::runtime_error(format("Can't open segment for reading: '%s'", path));
	fseek(file, 0, SEEK_END);
	auto const fsize = ftell(file);
	fclose(file);
	return fsize;
}

class LibavMuxHLSTS : public ModuleDynI {
	public:
		LibavMuxHLSTS(KHost* host, HlsMuxConfigLibav* cfg)
			: m_host(host),
			  m_utcStartTime(cfg->utcStartTime),
			  segDuration(timescaleToClock(cfg->segDurationInMs, 1000)), hlsDir(cfg->baseDir), segBasename(cfg->baseName) {
			{
				MuxConfig muxCfg = {format("%s%s.m3u8", hlsDir, cfg->baseName), "hls", cfg->options};
				delegate = loadModule("LibavMux", m_host, &muxCfg);
			}
			addInput();
			outputSegment  = addOutput<OutputDefault>();
			outputManifest = addOutput<OutputDefault>();
		}

		void flush() override {
			while (post()) {}
		}

		void process() override {
			ensureDelegateInputs();

			int inputIdx = 0;
			Data data;
			while (!inputs[inputIdx]->tryPop(data)) {
				inputIdx++;
			}
			delegate->getInput(inputIdx)->push(data);

			if (data->getMetadata()->type == VIDEO_PKT) {
				const int64_t DTS = data->getMediaTime();
				if (firstDTS == -1) {
					firstDTS = DTS;
					startsWithRAP = safe_cast<const DataAVPacket>(data)->getPacket()->flags & AV_PKT_FLAG_KEY;
				}

				if (DTS >= (segIdx + 1) * segDuration + firstDTS) {
					auto meta = make_shared<MetadataFile>(SEGMENT);
					meta->durationIn180k = segDuration;
					meta->filename = format("%s%s%s.ts", hlsDir, segBasename, segIdx);
					meta->startsWithRAP = startsWithRAP;

					switch (data->getMetadata()->type) {
					case AUDIO_PKT:
						meta->sampleRate = safe_cast<const MetadataPktAudio>(data->getMetadata())->sampleRate; break;
					case VIDEO_PKT: {
						auto const res = safe_cast<const MetadataPktVideo>(data->getMetadata())->resolution;
						meta->resolution = res;
						break;
					}
					default: assert(0);
					}

					schedule({ (int64_t)m_utcStartTime->query() + data->getMediaTime(), meta, segIdx });

					/*next segment*/
					startsWithRAP = safe_cast<const DataAVPacket>(data)->getPacket()->flags & AV_PKT_FLAG_KEY;;
					segIdx++;
				}
			}

			postIfPossible();
		}

		IInput* getInput(int i) override {
			delegate->getInput(i);
			return ModuleDynI::getInput(i);
		}

	private:
		void ensureDelegateInputs() {
			auto const inputs = getNumInputs();
			auto const delegateInputs = delegate->getNumInputs();
			for (auto i = delegateInputs; i < inputs; ++i) {
				delegate->getInput(i);
			}
		}

		struct PostableSegment {
			int64_t pts;
			std::shared_ptr<MetadataFile> meta;
			int64_t segIdx;
		};

		void schedule(PostableSegment s) {
			segmentsToPost.push_back(s);
		}

		bool post() {
			if (segmentsToPost.empty())
				return false;

			auto s = segmentsToPost.front();
			if (fileExists(s.meta->filename)) {
				s.meta->filesize = fileSize(s.meta->filename);
			} else {
				m_host->log(Warning, format("Cannot post filename \"%s\": file does not exist.", s.meta->filename).c_str());
				return false;
			}

			auto data = outputSegment->allocData<DataRaw>(0);
			data->setMediaTime(s.pts);
			data->setMetadata(s.meta);
			outputSegment->post(data);
			segmentsToPost.erase(segmentsToPost.begin());

			/*update playlist*/
			{
				auto out = outputManifest->allocData<DataRaw>(0);

				auto metadata = make_shared<MetadataFile>(PLAYLIST);
				metadata->filename = hlsDir + segBasename + ".m3u8";

				out->setMetadata(metadata);
				outputManifest->post(out);
			}

			return true;
		}

		void postIfPossible() {
			if (segmentsToPost.empty())
				return;

			/*segment is complete when next segment exists*/
			auto s = segmentsToPost.front();
			if (!fileExists(format("%s%s%s.ts", hlsDir, segBasename, s.segIdx + 1)))
				return;

			post();
		}

		KHost* const m_host;
		IUtcStartTimeQuery* const m_utcStartTime;
		std::shared_ptr<IModule> delegate;
		OutputDefault *outputSegment, *outputManifest;
		std::vector<PostableSegment> segmentsToPost;
		int64_t firstDTS = -1, segDuration, segIdx = 0;
		std::string hlsDir, segBasename;
		bool startsWithRAP = false;
};

IModule* createObject(KHost* host, void* va) {
	auto config = (HlsMuxConfigLibav*)va;
	enforce(host, "LibavMuxHLSTS: host can't be NULL");
	enforce(config, "LibavMuxHLSTS: config can't be NULL");
	return createModule<LibavMuxHLSTS>(host, config).release();
}

auto const registered = Factory::registerModule("LibavMuxHLSTS", &createObject);
}
