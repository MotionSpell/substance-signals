#pragma once

#include "gpac_mux_mp4.hpp"

namespace Modules {
namespace Mux {

class GPACMuxMP4MSS : public GPACMuxMP4 {
	public:
		GPACMuxMP4MSS(IModuleHost* host, Mp4MuxConfigMss& cfg);

	private:
		void declareStreamVideo(const MetadataPktVideo* metadata) final;
		void declareStreamAudio(const MetadataPktAudio* metadata) final;
		void declareStreamSubtitle(const MetadataPktSubtitle* metadata) final;
		void startSegmentPostAction() final;

		std::string writeISMLManifest(std::string codec4CC, std::string codecPrivate, int64_t bitrate, int width, int height, uint32_t sampleRate, uint32_t channels, uint16_t bitsPerSample);
		std::string ISMLManifest;
		const std::string audioLang, audioName;
};

}
}
