#include <vector>
#include <algorithm> // is_sorted
#include "tests/tests.hpp"
#include "lib_modules/core/data.hpp"
#include "lib_modules/core/connection.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/decode/decoder.hpp"

using namespace Tests;
using namespace Modules;
using namespace std;

unittest("[DISABLED] LibavDemux => Decoder: output media times must increase") {

	vector<int64_t> mediaTimes;
	auto onPic = [&](Data data) {
		mediaTimes.push_back(data->getMediaTime());
	};

	MetadataPktVideo meta;
	meta.codec = "h264";

	auto demux = create<Demux::LibavDemux>("data/h264.ts");
	auto decoder = create<Decode::Decoder>(&meta);
	ConnectOutputToInput(demux->getOutput(0), decoder->getInput(0));
	ConnectOutput(decoder.get(), onPic);

	demux->process(nullptr);
	demux->flush();
	decoder->flush();

	ASSERT(is_sorted(mediaTimes.begin(), mediaTimes.end()));
}

