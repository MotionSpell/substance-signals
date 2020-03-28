#pragma once

#include "lib_modules/core/buffer.hpp" // Span
#include <memory>
#include <string>
#include <vector>

struct DashMpd;
struct AdaptationSet;

struct Representation {
	const AdaptationSet& set(DashMpd const * const mpd) const;
	std::string id;
	std::string codecs;
	std::string mimeType;
	int setIdx = -1;
};

struct AdaptationSet {
	std::string media;
	int startNumber = 0;
	int duration = 0;
	int timescale = 1;
	std::string initialization;
	std::string contentType;
	std::string srd;
	std::vector<Representation> representations;
};

struct DashMpd {
	bool dynamic = false;
	int64_t availabilityStartTime = 0; // in ms
	int64_t publishTime = 0; // in ms
	int64_t periodDuration = 0; // in seconds
	int64_t minUpdatePeriod = 0;
	std::vector<AdaptationSet> sets;
};

std::unique_ptr<DashMpd> parseMpd(span<const char> text);

