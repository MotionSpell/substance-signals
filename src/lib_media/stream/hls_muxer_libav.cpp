#include "hls_muxer_libav.hpp"

#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/loader.hpp"
#include "../mux/libav_mux.hpp"
#include "../common/libav.hpp"
#include "../common/metadata_file.hpp"
#include "lib_utils/format.hpp"

#include <cassert>

using namespace Modules;

namespace {


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
			addInput(this);
			outputSegment  = addOutput<OutputDataDefault<DataRaw>>();
			outputManifest = addOutput<OutputDataDefault<DataRaw>>();
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
				}
				if (DTS >= (segIdx + 1) * segDuration + firstDTS) {
					auto const fn = format("%s%s.ts", segBasename, segIdx);

					{
						auto out = outputSegment->getBuffer(0);
						out->setMediaTime(m_utcStartTime->query() + data->getMediaTime());

						auto metadata = make_shared<MetadataFile>(SEGMENT);
						metadata->durationIn180k = segDuration;
						metadata->filename = hlsDir + fn;
						metadata->filesize = fileSize(hlsDir + fn);

						switch (data->getMetadata()->type) {
						case AUDIO_PKT:
							metadata->sampleRate = safe_cast<const MetadataPktAudio>(data->getMetadata())->sampleRate; break;
						case VIDEO_PKT: {
							auto const res = safe_cast<const MetadataPktVideo>(data->getMetadata())->resolution;
							metadata->resolution = res;
							break;
						}
						default: assert(0);
						}
						out->setMetadata(metadata);

						outputSegment->post(out);
					}

					{
						auto out = outputManifest->getBuffer(0);

						auto metadata = make_shared<MetadataFile>(PLAYLIST);
						metadata->filename = hlsDir + segBasename + ".m3u8";

						out->setMetadata(metadata);
						outputManifest->post(out);
					}

					segIdx++;
				}
			}
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

		KHost* const m_host;
		IUtcStartTimeQuery* const m_utcStartTime;
		std::shared_ptr<IModule> delegate;
		OutputDataDefault<DataRaw> *outputSegment, *outputManifest;
		int64_t firstDTS = -1, segDuration, segIdx = 0;
		std::string hlsDir, segBasename;
};

IModule* createObject(KHost* host, void* va) {
	auto config = (HlsMuxConfigLibav*)va;
	enforce(host, "LibavMuxHLSTS: host can't be NULL");
	enforce(config, "LibavMuxHLSTS: config can't be NULL");
	return createModule<LibavMuxHLSTS>(host, config).release();
}

auto const registered = Factory::registerModule("LibavMuxHLSTS", &createObject);
}

