#pragma once

#include <vector>
#include <stdexcept>
#include <string>
#include "lib_modules/core/database.hpp"

namespace Modules {

struct Page {
	struct Line {
		std::string text;
		std::string color = "#ffffff";
		bool doubleHeight = false;

		// default values inherited from teletext
		int row = 24;
		int col = 0;
	};
	int64_t showTimestamp = 0;
	int64_t hideTimestamp = 0;
	std::vector<Line> lines;

	std::string toString() const {
		std::string r;

		for(auto& ss : lines) {
			r += ss.text;
			if (&ss != &lines.back())
				r += "\n";
		}

		return r;
	}
};

struct DataSubtitle : DataBase {
	DataSubtitle(size_t size) {
		if (size > 0)
			throw std::runtime_error("Forbidden operation. DataSubtitle requested size must be 0.");
	}
	Page page;
};

}
