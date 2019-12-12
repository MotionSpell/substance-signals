#include "mpeg_dash_parser.hpp"
#include "sax_xml_parser.hpp"
#include <algorithm> // max
#include <time.h>

using namespace std;

int64_t parseIso8601Period(string input);

namespace {

// 'timegm' is GNU/Linux only, use a portable one.
time_t my_timegm(struct tm * t) {
	auto const MONTHSPERYEAR = 12;
	static const int cumulatedDays[MONTHSPERYEAR] =
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

	long year = 1900 + t->tm_year + t->tm_mon / MONTHSPERYEAR;
	time_t r = (year - 1970) * 365 + cumulatedDays[t->tm_mon % MONTHSPERYEAR];
	r += (year - 1968) / 4;
	r -= (year - 1900) / 100;
	r += (year - 1600) / 400;
	if ((year % 4) == 0
	    && ((year % 100) != 0 || (year % 400) == 0)
	    && (t->tm_mon % MONTHSPERYEAR) < 2)
		r--;
	r += t->tm_mday - 1;
	r *= 24;
	r += t->tm_hour;
	r *= 60;
	r += t->tm_min;
	r *= 60;
	r += t->tm_sec;

	if (t->tm_isdst == 1)
		r -= 3600;

	return r;
}

// "2019-03-04T15:32:17"
static int64_t parseDate(string s) {
	int year, month, day, hour, minute, second;
	int ret = sscanf(s.c_str(), "%04d-%02d-%02dT%02d:%02d:%02d",
	        &year,
	        &month,
	        &day,
	        &hour,
	        &minute,
	        &second);
	if(ret != 6)
		throw runtime_error("Invalid date '" + s + "'");

	tm date {};
	date.tm_year = year - 1900;
	date.tm_mon = month - 1;
	date.tm_mday = day;
	date.tm_hour = hour;
	date.tm_min = minute;
	date.tm_sec = second;

	return my_timegm(&date);
}

} //anonymous namespace

DashMpd parseMpd(span<const char> text) {
	DashMpd r {};
	DashMpd* mpd = &r;

	auto onNodeStart = [mpd](string name, map<string, string>& attr) {
		if(name == "AdaptationSet") {
			auto set = make_unique<AdaptationSet>();
			set->contentType = attr["contentType"];
			mpd->sets.push_back(move(set));
		} else if(name == "Period") {
			if(!attr["duration"].empty())
				mpd->periodDuration = parseIso8601Period(attr["duration"]);
		} else if(name == "MPD") {
			mpd->dynamic = attr["type"] == "dynamic";

			if(!attr["availabilityStartTime"].empty())
				mpd->availabilityStartTime = parseDate(attr["availabilityStartTime"]);

			if(!attr["publishTime"].empty())
				mpd->publishTime = parseDate(attr["publishTime"]);

			if(!attr["minimumUpdatePeriod"].empty())
				mpd->minUpdatePeriod = parseIso8601Period(attr["minimumUpdatePeriod"]);
		} else if(name == "SegmentTemplate") {
			auto& set = mpd->sets.back();

			if(attr.find("initialization") != attr.end())
				set->initialization = attr["initialization"];

			if(attr.find("media") != attr.end())
				set->media = attr["media"];

			int startNumber = atoi(attr["startNumber"].c_str());
			set->startNumber = std::max<int>(set->startNumber, startNumber);
			if(attr.find("duration") != attr.end())
				set->duration = atoi(attr["duration"].c_str());
			if(!attr["timescale"].empty())
				set->timescale = atoi(attr["timescale"].c_str());
		} else if(name == "Representation") {
			Representation rep;
			rep.id = attr["id"];
			rep.codecs = attr["codecs"];
			rep.mimeType = attr["mimeType"];

			auto set = mpd->sets.back().get();
			rep.set = set;
			set->representations.push_back(rep);
		}
	};

	saxParse(text, onNodeStart);

	return r;
}

