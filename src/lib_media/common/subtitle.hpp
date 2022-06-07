#pragma once

#include <vector>
#include <stdexcept>
#include <string>
#include "lib_modules/core/database.hpp"

namespace Modules {

struct Page {
	int64_t showTimestamp = 0;
	int64_t hideTimestamp = 0;

	struct Line {
		std::string text;
		std::string color = "#ffffff";
		std::string bgColor = "#000000c2"; // TTML BasicDE: black with an opacity of 76% and a transparency of 24%

		//BasicDE: To leave space between the text in adjacent
		//rows, the font size for double height is not 200% but 160%. The value of 125% for
		//tts:lineHeight defines a line-height that is 125% of the computed font-size
		bool doubleHeight = false;

		// default values inherited from teletext
		int row = 24; // last line
		int col = 0;
	};
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
	std::shared_ptr<DataBase> clone() const override {
		auto dataSub = std::make_shared<DataSubtitle>(0);
		std::shared_ptr<DataBase> clone = dataSub;
		DataBase::clone(this, clone.get());
		dataSub->page = page;
		return clone;
	}
	Page page;
};

}
