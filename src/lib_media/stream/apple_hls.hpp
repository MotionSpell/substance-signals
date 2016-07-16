#pragma once

#include "adaptive_streaming_common.hpp"
#include <sstream>

namespace Modules {
namespace Stream {

class Apple_HLS : public AdaptiveStreamingCommon {
	public:
		Apple_HLS(const std::string &m3u8Path, Type type, uint64_t segDurationInMs);
		virtual ~Apple_HLS() {}

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
		std::stringstream playlistMaster;
		std::string playlistMasterPath;

};

}
}
