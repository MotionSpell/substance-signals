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
	int currNumber = 0;
	Fraction segmentDuration;
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
	//GET MPD FROM HTTP
	auto mpdAsText = m_source->get(url.c_str());
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
		stream->out = addOutput<OutputDefault>();
		stream->out->setMetadata(meta);
		stream->set = &set;
		stream->segmentDuration = Fraction(set.duration, set.timescale);

		m_streams.push_back(move(stream));
	}

	for(auto& stream : m_streams) {
		stream->currNumber  = stream->set->startNumber;
		if(mpd->dynamic) {
			auto now = (int64_t)getUTC();
			stream->currNumber += int(stream->segmentDuration.inverse() * (now - mpd->availabilityStartTime));
			// HACK: add one segment latency
			stream->currNumber -= 2;
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

		auto chunk = m_source->get(url.c_str());
		if(chunk.empty()) {
			if(mpd->dynamic) {
				stream->currNumber--; // too early, retry
				continue;
			}
			m_host->log(Error, format("can't download file: '%s'", url).c_str());
			m_host->activate(false);
		}

		auto data = make_shared<DataRaw>(chunk.size());
		memcpy(data->data().ptr, chunk.data(), chunk.size());
		stream->out->post(data);
	}

	m_initializationChunkSent = true;
}

}
}

///////////////////////////////////////////////////////////////////////////////
// nothing above this line should depend upon gpac

#include "../common/sax_xml_parser.hpp"

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
		} else if(name == "SegmentTemplate") {
			auto& set = mpd->sets.back();
			set.initialization = attr["initialization"];
			set.media = attr["media"];
			set.startNumber = atoi(attr["startNumber"].c_str());
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

