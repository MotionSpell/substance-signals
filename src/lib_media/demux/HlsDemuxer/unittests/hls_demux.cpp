#include "lib_media/demux/HlsDemuxer/hls_demux.hpp"
#include "tests/tests.hpp"
#include "lib_modules/utils/loader.hpp"
#include <vector>
#include <map>
#include <string>

using namespace std;
using namespace Tests;
using namespace Modules;
using namespace In;

namespace {
struct LocalFileSystem : IFilePuller {
	vector<uint8_t> get(const char* szUrl) override {
		auto url = string(szUrl);
		requests.push_back(url);
		if(resources.find(url) == resources.end())
			return {};
		return {resources[url].begin(), resources[url].end()};
	}

	map<string, string> resources;
	vector<string> requests;
};

}

unittest("hls demux: download main playlist") {
	LocalFileSystem fs;
	fs.resources["playlist.m3u8"] = "sub.m3u8\n";

	HlsDemuxConfig cfg {};
	cfg.url = "playlist.m3u8";
	cfg.filePuller = &fs;
	auto demux = loadModule("HlsDemuxer", &NullHost, &cfg);
	demux->process();
	ASSERT_EQUALS(
	vector<string>({
		"playlist.m3u8",
		"sub.m3u8",
	}),
	fs.requests);
}

