#pragma once

#include "gpac_mux_mp4.hpp"

namespace Modules {
namespace Mux {

class GPACMuxMP4MSS : public GPACMuxMP4 {
public:
	GPACMuxMP4MSS(const std::string &baseName, uint64_t segmentDurationInMs, const std::string &audioLang = "", const std::string &audioName = "");

private:
	void declareStreamVideo(std::shared_ptr<const MetadataPktLibavVideo> stream) final;
	void declareStreamAudio(std::shared_ptr<const MetadataPktLibavAudio> metadata) final;
	void declareStreamSubtitle(std::shared_ptr<const MetadataPktLibavSubtitle> metadata) final;
	void startSegmentPostAction() final;

	std::string writeISMLManifest(std::string codec4CC, std::string codecPrivate, int64_t bitrate, int width, int height, uint32_t sampleRate, uint32_t channels, uint16_t bitsPerSample);
	std::string ISMLManifest;
	const std::string audioLang, audioName;
};

}
}
