#pragma once

#include "adaptive_streaming_common.hpp"
#include "../common/gpacpp.hpp"
#include <vector>

namespace Modules {

struct DasherConfig {
	std::string mpdDir;
	std::string mpdName;
	Stream::AdaptiveStreamingCommon::Type type;
	uint64_t segDurationInMs;
	uint64_t timeShiftBufferDepthInMs = 0;
	uint64_t minUpdatePeriodInMs = 0;
	uint32_t minBufferTimeInMs = 0;
	std::vector<std::string> baseURLs {};
	std::string id = "id";
	int64_t initialOffsetInMs = 0;
	Stream::AdaptiveStreamingCommon::AdaptiveStreamingCommonFlags flags = Stream::AdaptiveStreamingCommon::None;
};

}
