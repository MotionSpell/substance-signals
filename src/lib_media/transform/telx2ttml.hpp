#pragma once

#include <string>
#include <functional>

struct TeletextToTtmlConfig {
	enum TimingPolicy {
		AbsoluteUTC,     //USP
		RelativeToMedia, //14496-30
		RelativeToSplit  //MSS
	};

	unsigned pageNum;
	std::string lang;
	uint64_t splitDurationInMs;
	uint64_t maxDelayBeforeEmptyInMs;
	TimingPolicy timingPolicy;
	std::function<int64_t()> getUtcPipelineStartTime = []() {
		return 0;
	};
};

