#pragma once

#include <string>
#include <vector>
#include "lib_utils/resolution.hpp"

struct Video { //FIXME: this can be factorized with other params
	Resolution res;
	int bitrate;
	int type;
};

struct Config {
	std::string input;
	std::string workingDir = ".";
	std::vector<Video> v;
	int segmentDurationInMs = 2000;
	int timeshiftInSegNum = 0;
	bool isLive = false;
	bool loop = false;
	bool ultraLowLatency = false;
	bool autoRotate = false;
	bool help = false;
};

