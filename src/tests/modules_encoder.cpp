#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

unittest("encoder: video simple") {
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));

	int numEncodedFrames = 0;
	auto onFrame = [&](Data data) {
		numEncodedFrames++;
	};

	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video);
	Connect(encode->getOutput(0)->getSignal(), onFrame);
	for (int i = 0; i < 50; ++i) {
		encode->process(picture);
	}

	ASSERT(numEncodedFrames > 0);
}

unittest("encoder: timestamps start at random values") {
	const std::vector<uint64_t> times = { Clock::Rate, 2*Clock::Rate, 3*Clock::Rate };
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	auto mux = create<Mux::GPACMuxMP4>("random_ts");
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));
	for (size_t i = 0; i < times.size(); ++i) {
		picture->setMediaTime(times[i]);
		encode->process(picture);
	}
	encode->flush();
	mux->flush();

	size_t i = 0;
	auto onFrame = [&](Data data) {
		ASSERT(data->getMediaTime() + times[0] == times[i]);
		i++;
	};
	auto demux = create<Demux::LibavDemux>("random_ts.mp4");
	Connect(demux->getOutput(0)->getSignal(), onFrame);
	demux->process(nullptr);
	demux->flush();
	ASSERT(i == times.size());
}

//TODO: logs on Error should be caught as exceptions in tests
unittest("GPAC mp4 mux: don't create empty fragments") {
	auto const segmentDurationInMs = 1000;
	const std::vector<uint64_t> times = { Clock::Rate, 0, 3*Clock::Rate };
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	auto mux = create<Mux::GPACMuxMP4>("tmp", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::Browsers | Mux::GPACMuxMP4::SegmentAtAny);
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));
	for (size_t i = 0; i < times.size(); ++i) {
		picture->setMediaTime(times[i]);
		encode->process(picture);
	}
	encode->flush();
	mux->flush();
}

//TODO: add a more complex test for each module!
