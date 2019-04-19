#pragma once

#include <vector>
#include <string>

struct Page {
	Page() {
		lines.push_back({});
	}

	std::string toString() const {
		std::string r;

		for(auto& ss : lines)
			r += ss + "\n";

		return r;
	}

	int64_t tsInMs = 0;
	int64_t startTimeInMs = 0;
	int64_t endTimeInMs = 0;
	int64_t showTimestamp = 0;
	int64_t hideTimestamp = 0;
	std::vector<std::string> lines;
};

