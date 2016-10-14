#pragma once

#include "adaptive_streaming_common.hpp"
#include "lib_gpacpp/gpacpp.hpp"
#include <list>

namespace Modules {
namespace Stream {

class MPEG_DASH : public AdaptiveStreamingCommon, public gpacpp::Init {
	public:
		MPEG_DASH(const std::string &mpdDir, const std::string &mpdName, Type type, uint64_t segDurationInMs, uint64_t timeShiftBufferDepthInMs = 0);
		virtual ~MPEG_DASH();

	private:
		std::unique_ptr<Quality> createQuality() const override;
		void generateManifest() override;
		void finalizeManifest() override;

		struct DASHQuality : public Quality {
			DASHQuality() : rep(nullptr) {}
			GF_MPD_Representation *rep;
			std::list<std::shared_ptr<const MetadataFile>> timeshiftSegments;
		};

		void ensureManifest();
		void writeManifest();
		std::unique_ptr<gpacpp::MPD> mpd;
		std::string mpdDir, mpdPath;
		uint64_t timeShiftBufferDepthInMs;
};

}
}
