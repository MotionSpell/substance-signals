#pragma once

#include "lib_utils/resolution.hpp"

struct Video { //FIXME: this can be factorized with other params
	Video(Resolution res, int bitrate, int type)
		: res(res), bitrate(bitrate), type(type) {}
	Resolution res;
	int bitrate;
	int type;
};

struct Config {
	std::string input, workingDir = ".", postCommand, id;
	std::vector<std::string> outputs;
	std::vector<Video> v;
	uint64_t seekTimeInMs = 0, segmentDurationInMs = 2000, timeshiftInSegNum = 0, minUpdatePeriodInMs = 0;
	uint32_t minBufferTimeInMs = 0;
	int64_t astOffset = 0;
	bool isLive = false, loop = false, ultraLowLatency = false, autoRotate = false, watermark = true;
};

