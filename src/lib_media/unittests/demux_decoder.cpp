#include <vector>
#include <algorithm> // is_sorted
#include "tests/tests.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_modules/core/database.hpp"
#include "lib_modules/core/connection.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/demux/libav_demux.hpp"

using namespace Tests;
using namespace Modules;
using namespace std;

unittest("LibavDemux => Decoder: output media times must increase") {

	vector<int64_t> mediaTimes;
	auto onPic = [&](Data data) {
		mediaTimes.push_back(data->getMediaTime());
	};

	DemuxConfig cfg;
	cfg.url = "data/h264.ts";
	auto demux = loadModule("LibavDemux", &NullHost, &cfg);
	auto decoder = loadModule("Decoder", &NullHost, (void*)(intptr_t)VIDEO_PKT);
	ConnectOutputToInput(demux->getOutput(0), decoder->getInput(0));
	ConnectOutput(decoder.get(), onPic);

	demux->process();
	demux->flush();
	decoder->flush();

	// output media times must increase
	ASSERT(is_sorted(mediaTimes.begin(), mediaTimes.end()));

	// all pictures must be there
	ASSERT_EQUALS(75, (int)mediaTimes.size());
}

