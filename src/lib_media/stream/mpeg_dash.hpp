#pragma once

#include "adaptive_streaming_common.hpp"
#include "lib_gpacpp/gpacpp.hpp"
#include <list>

namespace Modules {
namespace Stream {

class MPEG_DASH : public AdaptiveStreamingCommon, public gpacpp::Init {
	public:
		MPEG_DASH(const std::string &mpdDir, const std::string &mpdName, Type type, uint64_t segDurationInMs, uint64_t timeShiftBufferDepthInMs = 0, uint64_t minUpdatePeriodInMs = 0, uint32_t minBufferTimeInMs = 0, const std::vector<std::string> &baseURLs = std::vector<std::string>(), const std::string &id = "id", int64_t initialOffsetInMs = 0);
		virtual ~MPEG_DASH();

	private:
		std::unique_ptr<Quality> createQuality() const override;
		void generateManifest() override;
		void finalizeManifest() override;

		struct DASHQuality : public Quality {
			DASHQuality() : rep(nullptr) {}
			GF_MPD_Representation *rep;
			struct SegmentToDelete {
				SegmentToDelete(std::shared_ptr<const MetadataFile> file) : file(file) {}
				std::shared_ptr<const MetadataFile> file;
				int retry = 5;
			};
			std::list<SegmentToDelete> timeshiftSegments;
		};

		void ensureManifest();
		void writeManifest();
		bool moveFile(const std::string &src, const std::string &dst) const;
		std::string getPeriodID() const;
		std::unique_ptr<gpacpp::MPD> mpd;
		const std::string mpdDir, mpdPath;
		const std::vector<std::string> baseURLs;
		const uint64_t minUpdatePeriodInMs, timeShiftBufferDepthInMs;
		const int64_t initialOffsetInMs;
		const bool useSegmentTimeline = false;
};

}
}
