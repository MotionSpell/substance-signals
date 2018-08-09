#pragma once

#include "lib_utils/resolution.hpp"

struct Video { //FIXME: this can be factorized with other params
	Resolution res;
	int bitrate;
	int type;
};

struct Config {
	std::string input;
	std::string workingDir = ".";
	std::vector<std::string> outputs;
	std::vector<Video> v;
	uint64_t segmentDurationInMs = 2000;
	uint64_t timeshiftInSegNum = 0;
	bool isLive = false;
	bool loop = false;
	bool ultraLowLatency = false;
	bool autoRotate = false;
	bool watermark = true;
};

