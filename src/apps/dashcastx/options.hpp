#pragma once

#include "lib_media/common/picture.hpp"

struct Video {
	Video(Modules::Resolution res, unsigned bitrate) : res(res), bitrate(bitrate) {}
	Modules::Resolution res;
	unsigned bitrate;
};

struct AppOptions {
	std::string url;
	std::vector<Video> v;
	uint64_t segmentDurationInMs = 2000;
	bool isLive = false;
};

AppOptions processArgs(int argc, char const* argv[]);
