#include "lib_utils/tools.hpp"
#include "lib_utils/time.hpp" // getUTC
#include "lib_utils/log_sink.hpp"
#include "lib_utils/format.hpp"
#include "../common/metadata.hpp" // MetadataPkt
#include "mpeg_dash_input.hpp"
#include <vector>
#include <map>
#include <cstring> // memcpy
#include <cassert>
#include <algorithm> // min

std::string expandVars(std::string input, std::map<std::string,std::string> const& values);
int64_t parseIso8601Period(std::string input);

using namespace std;
using namespace Modules::In;

struct AdaptationSet {
	string media;
	int startNumber=0;
	int duration=0;
	int timescale=1;
	string representationId;
	string codecs;
	string initialization;
	string mimeType;
	string contentType;
};

struct DashMpd {
	bool dynamic = false;
	int64_t availabilityStartTime = 0; // in ms
	int64_t periodDuration = 0; // in seconds
	vector<AdaptationSet> sets;
};

static DashMpd parseMpd(span<const char> text);

namespace Modules {
namespace In {

struct MPEG_DASH_Input::Stream {
	OutputDefault* out = nullptr;
	AdaptationSet* set = nullptr;
	int64_t currNumber = 0;
	Fraction segmentDuration {};
};

static string dirName(string path) {
	auto i = path.rfind('/');
	if(i != path.npos)
		path = path.substr(0, i);
	return path;
}

static shared_ptr<IMetadata> createMetadata(AdaptationSet const& set) {
	if(set.mimeType == "audio/mp4" || set.contentType == "audio") {
		return make_shared<MetadataPkt>(AUDIO_PKT);
	} else if(set.mimeType == "video/mp4" || set.contentType == "video") {
		return make_shared<MetadataPkt>(VIDEO_PKT);
	} else {
		return nullptr;
	}
}

MPEG_DASH_Input::MPEG_DASH_Input(KHost* host, IFilePuller* source, std::string const& url)
	:  m_host(host), m_source(source) {
	m_host->activate(true);

	//GET MPD FROM HTTP
	auto mpdAsText = download(m_source, url.c_str());
	if(mpdAsText.empty())
		throw std::runtime_error("can't get mpd");
	m_mpdDirname = dirName(url);

	//PARSE MPD
	mpd = make_unique<DashMpd>();
	*mpd = parseMpd({(const char*)mpdAsText.data(), mpdAsText.size()});

	//DECLARE OUTPUT PORTS
	for(auto& set : mpd->sets) {
		auto meta = createMetadata(set);
		if(!meta) {
			m_host->log(Warning, format("Ignoring adaptation set with unrecognized mime type: '%s'", set.mimeType).c_str());
			continue;
		}

		auto stream = make_unique<Stream>();
		stream->out = addOutput();
		stream->out->setMetadata(meta);
		stream->set = &set;
		stream->segmentDuration = Fraction(set.duration, set.timescale);

		m_streams.push_back(move(stream));
	}

	for(auto& stream : m_streams) {
		stream->currNumber = stream->set->startNumber;
		if(mpd->dynamic) {
			auto now = (int64_t)getUTC();
			if(stream->segmentDuration.num == 0)
				throw runtime_error("No duration for stream");

			stream->currNumber += int64_t(stream->segmentDuration.inverse() * (now - mpd->availabilityStartTime));
			// HACK: add one segment latency
			stream->currNumber = std::max<int64_t>(stream->currNumber-2, stream->set->startNumber);
		}
	}
}

MPEG_DASH_Input::~MPEG_DASH_Input() {
}

void MPEG_DASH_Input::process() {
	for(auto& stream : m_streams) {
		auto& set = *stream->set;

		if(mpd->periodDuration) {
			if(stream->segmentDuration * (stream->currNumber - set.startNumber) >= mpd->periodDuration) {
				m_host->log(Info, "End of period");
				m_host->activate(false);
				return;
			}
		}

		string url;

		{
			map<string, string> vars;

			vars["RepresentationID"] = set.representationId;

			if(m_initializationChunkSent) {
				vars["Number"] = format("%s", stream->currNumber);
				stream->currNumber++;
				url = m_mpdDirname + "/" + expandVars(set.media, vars);
			} else {
				url = m_mpdDirname + "/" + expandVars(set.initialization, vars);
			}
		}

		m_host->log(Debug, format("wget: '%s'", url).c_str());

		bool empty = true;

		auto onBuffer = [&](SpanC chunk) {
			empty = false;

			auto data = make_shared<DataRaw>(chunk.len);
			memcpy(data->buffer->data().ptr, chunk.ptr, chunk.len);
			stream->out->post(data);
		};

		m_source->wget(url.c_str(), onBuffer);
		if(empty) {
			if(mpd->dynamic) {
				stream->currNumber--; // too early, retry
				continue;
			}
			m_host->log(Error, format("can't download file: '%s'", url).c_str());
			m_host->activate(false);
		}
	}

	m_initializationChunkSent = true;
}

}
}

///////////////////////////////////////////////////////////////////////////////

#include "../common/sax_xml_parser.hpp"
#include <time.h>

// 'timegm' is GNU/Linux only, use a portable one.

static time_t my_timegm(struct tm * t) {
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
int64_t parseDate(string s) {
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

DashMpd parseMpd(span<const char> text) {

	DashMpd r {};
	DashMpd* mpd = &r;

	auto onNodeStart = [mpd](std::string name, map<string, string>& attr) {
		if(name == "AdaptationSet") {
			AdaptationSet set;
			set.contentType = attr["contentType"];
			mpd->sets.push_back(set);
		} else if(name == "Period") {
			if(!attr["duration"].empty())
				mpd->periodDuration = parseIso8601Period(attr["duration"]);
		} else if(name == "MPD") {
			mpd->dynamic = attr["type"] == "dynamic";
			if(!attr["availabilityStartTime"].empty())
				mpd->availabilityStartTime = parseDate(attr["availabilityStartTime"]);
		} else if(name == "SegmentTemplate") {
			auto& set = mpd->sets.back();

			if(attr.find("initialization") != attr.end())
				set.initialization = attr["initialization"];

			if(attr.find("media") != attr.end())
				set.media = attr["media"];

			int startNumber = atoi(attr["startNumber"].c_str());
			set.startNumber = std::max<int>(set.startNumber, startNumber);
			if(attr.find("duration") != attr.end())
				set.duration = atoi(attr["duration"].c_str());
			if(!attr["timescale"].empty())
				set.timescale = atoi(attr["timescale"].c_str());
		} else if(name == "Representation") {
			auto& set = mpd->sets.back();
			set.representationId = attr["id"];
			set.codecs = attr["codecs"];
			set.mimeType = attr["mimeType"];
		}
	};

	saxParse(text, onNodeStart);

	return  r;
}

///////////////////////////////////////////////////////////////////////////////
// move this elsewhere

// usage example:
// map<string, string> vars;
// vars["Name"] = "john";
// assert("hello john" == expandVars("hello $Name$", vars));
string expandVars(string input, map<string,string> const& values) {

	int i=0;
	auto front = [&]() {
		return input[i];
	};
	auto pop = [&]() {
		return input[i++];
	};
	auto empty = [&]() {
		return i >= (int)input.size();
	};

	auto parseVarName = [&]() -> string {
		auto leadingDollar = pop();
		assert(leadingDollar == '$');

		string name;
		while(!empty() && front() != '$') {
			name += pop();
		}

		if(empty())
			throw runtime_error("unexpected end of string found when parsing variable name");

		pop(); // pop terminating '$'
		return name;
	};

	string r;

	while(!empty()) {
		auto const head = front();
		if(head == '$') {
			auto name = parseVarName();
			if(values.find(name) == values.end())
				throw runtime_error("unknown variable name '" + name + "'");

			r += values.at(name);
		} else {
			r += pop();
		}
	}

	return r;
}

