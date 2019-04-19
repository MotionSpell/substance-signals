#pragma once

#include "lib_modules/core/module.hpp" // KHost
#include "page.hpp"
#include <vector>

struct ITeletextParser {
	virtual ~ITeletextParser() = default;
	virtual std::vector<Page> parse(SpanC data, int64_t time) = 0;
};

ITeletextParser* createTeletextParser(Modules::KHost* host, int pageNum);
