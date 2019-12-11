#pragma once

#include "lib_modules/core/buffer.hpp" // Span
#include <memory>
#include <string>
#include <vector>

struct AdaptationSet;

struct Representation {
	AdaptationSet const * set = nullptr;
	std::string id;
	std::string codecs;
	std::string mimeType;
};

struct AdaptationSet {
	std::string media;
	int startNumber=0;
	int duration=0;
	int timescale=1;
	std::string initialization;
	std::string contentType;
	std::vector<Representation> representations;
};

struct DashMpd {
	bool dynamic = false;
	int64_t availabilityStartTime = 0; // in ms
	int64_t publishTime = 0; // in ms
	int64_t periodDuration = 0; // in seconds
	std::vector<std::unique_ptr<AdaptationSet>> sets;
};

DashMpd parseMpd(span<const char> text);

