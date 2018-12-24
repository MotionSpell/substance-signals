#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"

using namespace Tests;
using namespace Modules;
using namespace In;

struct LocalFilesystem : IFilePuller {
	std::vector<uint8_t> get(const char* szUrl) override {
		auto url = std::string(szUrl);
		requests.push_back(url);
		if(resources.find(url) == resources.end()) {
			return {};
		}
		return {resources[url].begin(), resources[url].end()};
	}

	std::map<std::string, std::string> resources;
	std::vector<std::string> requests;
};

unittest("mpeg_dash_input: fail to get MPD") {
	LocalFilesystem source;
	ASSERT_THROWN(create<MPEG_DASH_Input>(&NullHost, &source, "http://toto.mpd"));
}

unittest("mpeg_dash_input: get MPD") {
	static auto const MPD = R"|(
<?xml version="1.0"?>
<MPD>
  <Period>
    <AdaptationSet>
      <ContentComponent id="1" />
      <SegmentTemplate initialization="video-init.mp4" media="video-$Number$.m4s" startNumber="1" />
      <Representation id="video" mimeType="video/mp4"/>
    </AdaptationSet>
    <AdaptationSet contentType="audio">
      <ContentComponent id="1"/>
      <SegmentTemplate initialization="audio-init.mp4" media="audio-$Number$.m4s" startNumber="1" />
      <Representation id="audio" />
    </AdaptationSet>
  </Period>
</MPD>)|";

	LocalFilesystem source;
	source.resources["http://toto.mpd"] = MPD;
	auto dash = create<MPEG_DASH_Input>(&NullHost, &source, "http://toto.mpd");
	ASSERT_EQUALS(2, dash->getNumOutputs());
}

unittest("mpeg_dash_input: get MPD, one input") {
	static auto const MPD = R"|(
<?xml version="1.0"?>
<MPD>
  <Period>
    <AdaptationSet>
      <ContentComponent id="1"/>
      <SegmentTemplate initialization="audio-init.mp4" media="audio-$Number$.m4s" startNumber="10" />
      <Representation id="audio" mimeType="audio/mp4"/>
    </AdaptationSet>
  </Period>
</MPD>)|";
	LocalFilesystem source;
	source.resources["http://single.mpd"] = MPD;
	auto dash = create<MPEG_DASH_Input>(&NullHost, &source, "http://single.mpd");
	ASSERT_EQUALS(1, dash->getNumOutputs());
}

unittest("mpeg_dash_input: get chunks") {
	static auto const MPD = R"|(
<?xml version="1.0"?>
<MPD>
  <Period duration="PT30S">
    <AdaptationSet>
      <Representation id="88" mimeType="unknown"/>
    </AdaptationSet>
    <AdaptationSet>
      <ContentComponent id="1"/>
      <SegmentTemplate initialization="init-$RepresentationID$.mp4" media="sub/x$Number$y$RepresentationID$z" startNumber="3" duration="10"/>
      <Representation id="77" mimeType="audio/mp4"/>
    </AdaptationSet>
  </Period>
</MPD>)|";
	LocalFilesystem source;
	source.resources["main/live.mpd"] = MPD;
	source.resources["main/init-77.mp4"] = "initdata";
	source.resources["main/sub/x3y77z"] = "data3";
	source.resources["main/sub/x4y77z"] = "data4";
	source.resources["main/sub/x5y77z"] = "data5";
	auto dash = create<MPEG_DASH_Input>(&NullHost, &source, "main/live.mpd");
	int chunkCount = 0;
	auto receive = [&](Data data) {
		++chunkCount;
		ASSERT(chunkCount < 10);
		(void)data;
	};
	ConnectOutput(dash.get(), receive);

	for(int i=0; i < 100; ++i)
		dash->process();

	ASSERT_EQUALS(
	std::vector<std::string>( {
		"main/live.mpd",
		"main/init-77.mp4",
		"main/sub/x3y77z",
		"main/sub/x4y77z",
		"main/sub/x5y77z",
	}),
	source.requests);
}

std::unique_ptr<IFilePuller> createHttpSource();

secondclasstest("mpeg_dash_input: get MPD from remote server") {
	auto url = "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live/mp4-live-mpd-AV-NBS.mpd";
	auto source = createHttpSource();
	auto dash = create<MPEG_DASH_Input>(&NullHost, source.get(), url);
	ASSERT_EQUALS(2, dash->getNumOutputs());
}

secondclasstest("mpeg_dash_input: non-existing MPD") {
	auto url = "http://example.com/this_url_doesnt_exist_121324315235";
	auto source = createHttpSource();
	ASSERT_THROWN(create<MPEG_DASH_Input>(&NullHost, source.get(), url));
}

