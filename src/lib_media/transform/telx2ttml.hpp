#pragma once

#include <string>
#include <functional>

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
	std::function<int64_t()> getUtcPipelineStartTime = []() {
		return 0;
	};
};

