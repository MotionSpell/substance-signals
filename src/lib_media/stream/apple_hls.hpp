#pragma once

#include "adaptive_streaming_common.hpp"

#define LIBAVMUXHLS //FIXME: see https://git.gpac-licensing.com/rbouqueau/fk-encode/issues/18 ; the libav muxer doesn't signal new segments*/
#include "../mux/libav_mux.hpp"
#include <lib_modules/utils/helper.hpp>
#include "lib_utils/clock.hpp"

namespace Modules {
namespace Stream {

#ifdef LIBAVMUXHLS
class LibavMuxHLS : public ModuleDynI {
public:
	LibavMuxHLS(bool isLowLatency, uint64_t segDurationInMs, const std::string &baseName, const std::string &fmt, const std::string &options = "")
	: segDuration(timescaleToClock(segDurationInMs, 1000)), segBasename(baseName) {
		if (fmt != "hls")
			error("HLS only!");
		if (isLowLatency) {
			delegate = uptr(createModule<Mux::LibavMux>(Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY, baseName, fmt, options));
		} else {
			delegate = uptr(createModule<Mux::LibavMux>(Modules::ALLOC_NUM_BLOCKS_DEFAULT, baseName, fmt, options));
		}
		addInput(new Input<DataAVPacket>(this));
		outputSegment  = addOutput<OutputDataDefault<DataAVPacket>>();
		outputManifest = addOutput<OutputDataDefault<DataAVPacket>>();
	}

	virtual ~LibavMuxHLS() {}

	void process() override {
		auto data = getInput(0)->pop();
		delegate->getInput(0)->push(data);
		delegate->process();

		const int64_t PTS = data->getMediaTime();
		if (firstPTS == -1) {
			firstPTS = PTS;
		}
		if (PTS >= (segIdx + 1) * segDuration + firstPTS) {
			auto const fn = format("%s%s.ts", segBasename, segIdx);
			auto file = fopen(fn.c_str(), "rt");
			if (!file)
				throw error(format("Can't open file for reading: %s", fn));
			fseek(file, 0, SEEK_END);
			auto const fsize = ftell(file);

			auto out = outputSegment->getBuffer(0);
			auto metadata = std::make_shared<MetadataFile>(fn, data->getMetadata()->getStreamType(), "", "", segDuration, fsize, 1, false);
			switch (data->getMetadata()->getStreamType()) {
			case AUDIO_PKT: metadata->sampleRate = safe_cast<const MetadataPktLibavAudio>(data->getMetadata())->getSampleRate(); break;
			case VIDEO_PKT: {
				auto const res = safe_cast<const MetadataPktLibavVideo>(data->getMetadata())->getResolution();
				metadata->resolution[0] = res.width;
				metadata->resolution[1] = res.height;
				break;
			}
			default: assert(0);
			}
			out->setMetadata(metadata);
			outputSegment->emit(out);

			out = outputManifest->getBuffer(0);
			metadata = std::make_shared<MetadataFile>(format("%s.m3u8", segBasename), PLAYLIST, "", "", 0, 0, 1, false);
			out->setMetadata(metadata);
			outputManifest->emit(out);
			segIdx++;
		}
	}

private:
	std::unique_ptr<Modules::Mux::LibavMux> delegate;
	OutputDataDefault<DataAVPacket> *outputSegment, *outputManifest;
	int64_t firstPTS = -1, segDuration, segIdx = 0;
	std::string segBasename;
};
#endif /*LIBAVMUXHLS*/

class Apple_HLS : public AdaptiveStreamingCommon {
public:
	Apple_HLS(const std::string &m3u8Path, Type type, uint64_t segDurationInMs);
	virtual ~Apple_HLS();

private:
	std::unique_ptr<Quality> createQuality() const override;
	void generateManifest() override;
	void finalizeManifest() override;

	void updateManifestVoDVariants();
	void generateManifestVariant();
	struct HLSQuality : public Quality {
		HLSQuality() {}
		std::stringstream playlistVariant;
		std::vector<std::string> segmentPaths;
	};

	void generateManifestMaster();
	std::string playlistMasterPath;
};

}
}
