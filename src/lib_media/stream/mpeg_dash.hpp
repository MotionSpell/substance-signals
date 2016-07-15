#pragma once

#include "adaptive_streaming_common.hpp"
#include "lib_gpacpp/gpacpp.hpp"

namespace Modules {
namespace Stream {

class MPEG_DASH : public AdaptiveStreamingCommon, public gpacpp::Init {
	public:
		MPEG_DASH(const std::string &mpdPath, Type type, uint64_t segDurationInMs);
		virtual ~MPEG_DASH() {}

	private:
		virtual void ensureManifest();
		virtual void generateManifest() override;
		virtual void finalizeManifest() override;

		std::unique_ptr<gpacpp::MPD> mpd;
		std::string mpdPath;
};

}
}
