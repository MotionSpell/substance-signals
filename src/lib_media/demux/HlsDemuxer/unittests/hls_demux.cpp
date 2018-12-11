#include "lib_media/demux/HlsDemuxer/hls_demux.hpp"
#include "tests/tests.hpp"
#include "lib_modules/utils/loader.hpp"
#include <vector>
#include <string>

using namespace std;
using namespace Tests;
using namespace Modules;

namespace {
struct LocalFileSystem : Modules::In::IFilePuller {
	std::vector<uint8_t> get(const char* url) {
		requests.push_back(url);
		return {};
	}
	vector<string> requests;
};
}

unittest("hls demux: simple") {
	LocalFileSystem fs;
	HlsDemuxConfig cfg {};
	cfg.url = "playlist.m3u8";
	cfg.filePuller = &fs;
	auto demux = loadModule("HlsDemuxer", &NullHost, &cfg);
	demux->process();
	ASSERT_EQUALS(vector<string>({"playlist.m3u8"}), fs.requests);
}

