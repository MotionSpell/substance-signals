#pragma once

#include <vector>
#include <string>

struct Page {
	int64_t startTimeInMs = 0;
	int64_t endTimeInMs = 0;
	int64_t showTimestamp = 0;
	int64_t hideTimestamp = 0;
	std::vector<std::string> lines;
};

