#pragma once

#include <string>
#include "../common/utc_start_time.hpp"

struct TeletextToTtmlConfig {
	enum TimingPolicy {
		AbsoluteUTC,     //USP
		RelativeToMedia, //14496-30
		RelativeToSplit  //MSS
	};

	unsigned pageNum = 0;
	std::string lang = "en";
	uint64_t splitDurationInMs = 1000;
	uint64_t maxDelayBeforeEmptyInMs = 2000;
	TimingPolicy timingPolicy = TimingPolicy::RelativeToSplit;
	IUtcStartTimeQuery* utcStartTime = &g_NullStartTime;
};

