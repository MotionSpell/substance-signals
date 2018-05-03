#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_utils/tools.hpp"
#include <iostream> // std::cerr

using namespace Tests;
using namespace Modules;

unittest("encoder: video simple") {
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));

	int numEncodedFrames = 0;
	auto onFrame = [&](Data /*data*/) {
		numEncodedFrames++;
	};

	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video);
	Connect(encode->getOutput(0)->getSignal(), onFrame);
	for (int i = 0; i < 50; ++i) {
		encode->process(picture);
	}

	ASSERT(numEncodedFrames > 0);
}

unittest("H265 encode and GPAC mp4 mux") {
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	p.avcodecCustom = "-vcodec libx265";
	try {
		auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
		auto mux = create<Mux::GPACMuxMP4>("tmp");
		ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));

		std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));
		encode->process(picture);
		encode->flush();
		mux->flush();
	} catch (std::exception const &e) {
		std::cerr << "No support for \"" << p.avcodecCustom << "\". Skipping test (" << e.what() << ")" << std::endl;
	}
}

void RAPTest(const Fraction fps, const std::vector<uint64_t> &times, const std::vector<bool> &RAPs) {
	Encode::LibavEncode::Params p;
	p.frameRate = fps;
	p.GOPSize = fps;
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	size_t i = 0;
	auto onFrame = [&](Data data) {
		if (i < RAPs.size()) {
			auto pkt = safe_cast<const DataAVPacket>(data)->getPacket();
			ASSERT((pkt->flags & AV_PKT_FLAG_KEY) == RAPs[i]);
		}
		i++;
	};
	Connect(encode->getOutput(0)->getSignal(), onFrame);
	for (size_t i = 0; i < times.size(); ++i) {
		picture->setMediaTime(times[i]);
		encode->process(picture);
	}
	encode->flush();
	ASSERT(i == RAPs.size());
}

unittest("encoder: RAP placement (25/1 fps)") {
	const std::vector<uint64_t> times = { 0, IClock::Rate / 2, IClock::Rate, IClock::Rate * 3 / 2, IClock::Rate * 2 };
	const std::vector<bool> RAPs = { true, false, true, false, true };
	RAPTest(Fraction(25, 1), times, RAPs);
}

unittest("encoder: RAP placement (30000/1001 fps)") {
	const std::vector<uint64_t> times = { 0, IClock::Rate/2, IClock::Rate, IClock::Rate*3/2, IClock::Rate*2 };
	const std::vector<bool> RAPs = { true, false, true, false, true };
	RAPTest(Fraction(30000, 1001), times, RAPs);
}

unittest("encoder: RAP placement (incorrect timings)") {
	const std::vector<uint64_t> times = { 0, 0, IClock::Rate };
	const std::vector<bool> RAPs = { true, false, true };
	RAPTest(Fraction(25, 1), times, RAPs);
}

#ifdef ENABLE_FAILING_TESTS
unittest("GPAC mp4 mux: don't create empty fragments") {
	auto const segmentDurationInMs = 1000;
	const std::vector<uint64_t> times = { IClock::Rate, 0, 3*IClock::Rate };
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	auto mux = create<Mux::GPACMuxMP4>("chrome", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::Browsers | Mux::GPACMuxMP4::SegmentAtAny);
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));
	for (size_t i = 0; i < times.size(); ++i) {
		picture->setMediaTime(times[i]);
		encode->process(picture);
	}
	encode->flush();
	mux->flush();

	/*TODO: find segment names: auto demux = create<Demux::LibavDemux>("tmp.mp4");
	Connect(demux->getOutput(0)->getSignal(), onFrame);
	demux->process(nullptr);
	ASSERT(i == times.size());*/
}
#endif

//TODO: add a more complex test for each module!
