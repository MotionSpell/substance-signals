#pragma once

#include <string>
#include "lib_media/common/utc_start_time.hpp"

struct TeletextToTtmlConfig {
	enum TimingPolicy {
		AbsoluteUTC,     //USP
		RelativeToMedia, //14496-30
		RelativeToSplit  //MSS
	};

	int pageNum = 0;
	std::string lang = "en";
	int splitDurationInMs = 1000;
	int maxDelayBeforeEmptyInMs = 2000;
	TimingPolicy timingPolicy = TimingPolicy::RelativeToSplit;
	IUtcStartTimeQuery* utcStartTime = &g_NullStartTime;
};

