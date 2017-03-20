#pragma once

#include "lib_utils/resolution.hpp"
#include <memory>
#include <string>
#include <vector>

struct Video {
	Video(Resolution res, unsigned bitrate, unsigned type)
		: res(res), bitrate(bitrate), type(type) {}
	Resolution res;
	unsigned bitrate;
	unsigned type = 0;
};

struct IConfig {
	virtual ~IConfig() {}
};

struct AppOptions : IConfig {
	std::string input, output1 = "", output2 = "", output3 = "", workingDir = ".", postCommand;
	std::vector<Video> v;
	uint64_t seekTimeInMs = 0, segmentDurationInMs = 2000, timeshiftInSegNum = 0, minUpdatePeriodInMs = 0, minBufferTimeInMs = 0;
	bool isLive = false, loop = false, ultraLowLatency = false, autoRotate = false;
};

std::unique_ptr<const IConfig> processArgs(int argc, char const* argv[]);
