#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"

using namespace Tests;
using namespace Modules;
using namespace In;

struct LocalFilesystem : IFilePuller {
	std::string get(std::string url) override {
		return resources[url];
	}

	std::map<std::string, std::string> resources;
};

unittest("mpeg_dash_input: get MPD") {
	static auto const MPD = R"|(
<?xml version="1.0"?>
<MPD>
  <Period>
    <AdaptationSet>
      <ContentComponent id="1" contentType="video"/>
      <SegmentTemplate initialization="video-init.mp4" media="video-$Number$.m4s" startNumber="1" />
      <Representation id="video"/>
    </AdaptationSet>
    <AdaptationSet>
      <ContentComponent id="1" contentType="audio"/>
      <SegmentTemplate initialization="audio-init.mp4" media="audio-$Number$.m4s" startNumber="1" />
      <Representation id="audio"/>
    </AdaptationSet>
  </Period>
</MPD>)|";

	LocalFilesystem source;
	source.resources["http://toto.mpd"] = MPD;
	auto dash = create<MPEG_DASH_Input>(&source, "http://toto.mpd");
	ASSERT_EQUALS(2u, dash->getNumOutputs());
}

unittest("mpeg_dash_input: get MPD, one input") {
	static auto const MPD = R"|(
<?xml version="1.0"?>
<MPD>
  <Period>
    <AdaptationSet>
      <ContentComponent id="1" contentType="audio"/>
      <SegmentTemplate initialization="audio-init.mp4" media="audio-$Number$.m4s" startNumber="10" />
      <Representation id="audio"/>
    </AdaptationSet>
  </Period>
</MPD>)|";
	LocalFilesystem source;
	source.resources["http://single.mpd"] = MPD;
	auto dash = create<MPEG_DASH_Input>(&source, "http://single.mpd");
	ASSERT_EQUALS(1u, dash->getNumOutputs());
}

std::unique_ptr<IFilePuller> createHttpSource();

secondclasstest("mpeg_dash_input: get MPD from remote server") {
	auto url = "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live/mp4-live-mpd-AV-NBS.mpd";
	auto source = createHttpSource();
	auto dash = create<MPEG_DASH_Input>(source.get(), url);
	ASSERT_EQUALS(2u, dash->getNumOutputs());
}

