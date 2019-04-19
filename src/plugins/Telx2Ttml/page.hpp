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

	int64_t tsInMs = 0, startTimeInMs = 0, endTimeInMs = 0, showTimestamp = 0, hideTimestamp = 0;
	std::vector<std::string> lines;
};

