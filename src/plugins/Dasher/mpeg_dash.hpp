#pragma once

#include <string>
#include <vector>
#include <lib_utils/time.hpp>

namespace Modules {

struct DasherConfig {
	std::string mpdDir;
	std::string mpdName;

	bool live = false;

	uint64_t segDurationInMs = 0;
	int64_t timeShiftBufferDepthInMs = 0;
	uint64_t minUpdatePeriodInMs = 0;
	uint32_t minBufferTimeInMs = 0;
	std::vector<std::string> baseURLs {};
	std::string id = "id";
	int64_t initialOffsetInMs = 0;

	bool segmentsNotOwned = false;
	bool presignalNextSegment = false;
	bool forceRealDurations = false;

	IUtcClock *utcClock = g_UtcClock;
};

}
