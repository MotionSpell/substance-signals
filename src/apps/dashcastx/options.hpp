#pragma once

#include "lib_media/common/picture.hpp"

struct Video {
	Video(Modules::Resolution res, unsigned bitrate, unsigned type)
		: res(res), bitrate(bitrate), type(type) {}
	Modules::Resolution res;
	unsigned bitrate;
	unsigned type = 0;
};

struct AppOptions {
	std::string input, output = "", workingDir = ".", postCommand;
	std::vector<Video> v;
	uint64_t seekTimeInMs = 0, segmentDurationInMs = 2000;
	bool isLive = false, ultraLowLatency = false, autoRotate = false;
};

AppOptions processArgs(int argc, char const* argv[]);
