#pragma once

#include "adaptive_streaming_common.hpp"
#include "lib_gpacpp/gpacpp.hpp"

namespace Modules {
namespace Stream {

class MPEG_DASH : public AdaptiveStreamingCommon, public gpacpp::Init {
	public:
		MPEG_DASH(const std::string &mpdPath, Type type, uint64_t segDurationInMs);
		virtual ~MPEG_DASH();

	private:
		std::unique_ptr<Quality> createQuality() const override;
		void generateManifest() override;
		void finalizeManifest() override;

		struct DASHQuality : public Quality {
			DASHQuality() : rep(nullptr) {}
			GF_MPD_Representation *rep;
		};

		void ensureManifest();
		void writeManifest();
		std::unique_ptr<gpacpp::MPD> mpd;
		std::string mpdPath;
};

}
}
