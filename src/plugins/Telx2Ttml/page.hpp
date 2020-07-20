#pragma once

#include <vector>
#include <string>

struct Page {
	struct Line {
		std::string text;
		std::string color = "#ffffff";
		bool doubleHeight = false;
		int row = 0;
		int col = 0;
	};
	int64_t startTimeInMs = 0;
	int64_t endTimeInMs = 0;
	int64_t showTimestamp = 0;
	int64_t hideTimestamp = 0;
	std::vector<Line> lines;
};

