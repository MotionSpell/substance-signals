#pragma once

#include <functional>
#include <string>

struct DemuxConfig {
	typedef std::function<int(uint8_t* buf, int bufSize)> ReadFunc;
	std::string url;
	bool loop = false;
	std::string avformatCustom;
	uint64_t seekTimeInMs = 0;
	std::string formatName;
	ReadFunc func = nullptr;
};

