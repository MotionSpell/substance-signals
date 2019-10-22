#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/common/metadata.hpp" //MetadataPkt
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>

using namespace Tests;
using namespace Modules;
using namespace In;

struct LocalFilesystem : IFilePuller {
	void wget(const char* szUrl, std::function<void(SpanC)> callback) override {
		std::unique_lock<std::mutex> lock(mutex);
		auto url = std::string(szUrl);
		requests.push_back(url);
		if(resources.find(url) == resources.end())
			return;

		callback({(uint8_t*)resources[url].data(), resources[url].size()});
	}

	std::map<std::string, std::string> resources;
	std::vector<std::string> requests;
	std::mutex mutex;
};

unittest("mpeg_dash_input: fail to get MPD") {
	LocalFilesystem source;
	ASSERT_THROWN(createModule<MPEG_DASH_Input>(&NullHost, &source, "http://toto.mpd"));
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
	auto dash = createModule<MPEG_DASH_Input>(&NullHost, &source, "http://toto.mpd");
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
      <Representation id="audio" mimeType="audio/mp4" codecs="codec_name_test"/>
    </AdaptationSet>
  </Period>
</MPD>)|";
	LocalFilesystem source;
	source.resources["http://single.mpd"] = MPD;
	auto dash = createModule<MPEG_DASH_Input>(&NullHost, &source, "http://single.mpd");

	auto meta = std::dynamic_pointer_cast<const MetadataPkt>(dash->getOutput(0)->getMetadata());
	ASSERT(meta);

	ASSERT_EQUALS("codec_name_test", meta->codec);
}

unittest("mpeg_dash_input: retrieve codec name") {
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
	auto dash = createModule<MPEG_DASH_Input>(&NullHost, &source, "http://single.mpd");
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
	auto dash = createModule<MPEG_DASH_Input>(&NullHost, &source, "main/live.mpd");
	int chunkCount = 0;
	auto receive = [&](Data data) {
		++chunkCount;
		ASSERT(chunkCount < 10);
		(void)data;
	};
	ConnectOutput(dash->getOutput(0), receive);

	for(int i=0; i < 100; ++i)
		dash->process();

	dash = nullptr;

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

unittest("mpeg_dash_input: only get available segments") {
	static auto const MPD = R"|(
<?xml version="1.0"?>
<MPD>
  <Period>
    <AdaptationSet>
      <SegmentTemplate
        initialization="$RepresentationID$/init.mp4"
        media="$RepresentationID$/$Number$.m4s"
        duration="10"/>

      <Representation id="medium" mimeType="audio/mp4">
        <SegmentTemplate startNumber="3" />
      </Representation>

      <Representation id="high" mimeType="audio/mp4">
        <SegmentTemplate startNumber="5" />
      </Representation>

      <Representation id="low" mimeType="audio/mp4">
        <SegmentTemplate startNumber="4" />
      </Representation>

    </AdaptationSet>
  </Period>
</MPD>)|";
	LocalFilesystem source;
	source.resources["main/manifest.mpd"] = MPD;
	source.resources["main/low/init.mp4"] = "a";
	source.resources["main/low/5.m4s"] = "a";
	source.resources["main/low/6.m4s"] = "a";
	source.resources["main/low/7.m4s"] = "a";
	auto dash = createModule<MPEG_DASH_Input>(&NullHost, &source, "main/manifest.mpd");
	auto receive = [&](Data) {};
	ConnectOutput(dash->getOutput(0), receive);

	for(int i=0; i < 5; ++i)
		dash->process();

	dash = nullptr;

	ASSERT_EQUALS(
	std::vector<std::string>( {
		"main/manifest.mpd",
		"main/low/init.mp4",
		"main/low/5.m4s",
		"main/low/6.m4s",
		"main/low/7.m4s",
		"main/low/8.m4s",
	}),
	source.requests);
}

unittest("mpeg_dash_input: get concurrent chunks") {
	struct BlockingSource : IFilePuller {
		BlockingSource() : counter(0) {}
		void wget(const char* szUrl, std::function<void(SpanC)> callback) override {
			if (counter == 0) {
				callback({(uint8_t*)MPD.data(), MPD.size()});
				counter++;
				return;
			}

			counter++;

			while (counter > 4)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));

			{
				std::unique_lock<std::mutex> lock(mutex);
				auto url = std::string(szUrl);
				requests.push_back(url);
			}
		}
		void unblock() {
			counter = 1;
		}

		std::atomic<int> counter;
		std::mutex mutex;
		std::vector<std::string> requests;
		const std::string MPD = R"|(
<?xml version="1.0"?>
<MPD>
  <Period duration="PT10S">
    <AdaptationSet>
      <SegmentTemplate initialization="init-$RepresentationID$.mp4" media="sub/x$Number$y$RepresentationID$z" startNumber="3" duration="0"/>
      <Representation id="77" mimeType="audio/mp4"/>
    </AdaptationSet>
    <AdaptationSet>
      <SegmentTemplate initialization="init-$RepresentationID$.mp4" media="sub/x$Number$y$RepresentationID$z" startNumber="3" duration="0"/>
	  <Representation id="88" mimeType="video/mp4"/>
    </AdaptationSet>
  </Period>
</MPD>)|";
	};

	BlockingSource source;
	auto dash = createModule<MPEG_DASH_Input>(&NullHost, &source, "main/live.mpd");

	dash->process(); //init
	dash->process(); //first segment
	source.unblock();
	dash = nullptr;

	//reorder
	std::vector<std::string> a77, v88;
	for (auto &url : source.requests) {
		if (url == "main/init-77.mp4" || url == "main/sub/x3y77z") a77.push_back(url);
		if (url == "main/init-88.mp4" || url == "main/sub/x3y88z") v88.push_back(url);
	}

	ASSERT_EQUALS(
	std::vector<std::string>( {
		"main/init-77.mp4",
		"main/sub/x3y77z"
	}), a77);
	ASSERT_EQUALS(
	std::vector<std::string>( {
		"main/init-88.mp4",
		"main/sub/x3y88z"
	}), v88);
}

std::unique_ptr<IFilePuller> createHttpSource();

secondclasstest("mpeg_dash_input: get MPD from remote server") {
	auto url = "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live/mp4-live-mpd-AV-NBS.mpd";
	auto source = createHttpSource();
	auto dash = createModule<MPEG_DASH_Input>(&NullHost, source.get(), url);
	ASSERT_EQUALS(2, dash->getNumOutputs());
}

secondclasstest("mpeg_dash_input: non-existing MPD") {
	auto url = "http://example.com/this_url_doesnt_exist_121324315235";
	auto source = createHttpSource();
	ASSERT_THROWN(createModule<MPEG_DASH_Input>(&NullHost, source.get(), url));
}

