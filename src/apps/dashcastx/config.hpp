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
	std::string input;
	std::string workingDir = ".";
	std::string id;
	std::vector<std::string> outputs;
	std::vector<Video> v;
	uint64_t seekTimeInMs = 0;
	uint64_t segmentDurationInMs = 2000;
	uint64_t timeshiftInSegNum = 0;
	uint64_t minUpdatePeriodInMs = 0;
	uint32_t minBufferTimeInMs = 0;
	int64_t astOffset = 0;
	bool isLive = false;
	bool loop = false;
	bool ultraLowLatency = false;
	bool autoRotate = false;
	bool watermark = true;
};

