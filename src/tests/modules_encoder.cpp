#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/demux/gpac_demux_mp4_simple.hpp"
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

template<typename T>
void checkTimestamps(const std::vector<int64_t> &timesIn, const std::vector<int64_t> &timesOut) {
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	auto mux = create<Mux::GPACMuxMP4>("random_ts");
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));
	for (size_t i = 0; i < timesIn.size(); ++i) {
		picture->setMediaTime(timesIn[i]);
		encode->process(picture);
	}
	encode->flush();
	mux->flush();

	size_t i = 0;
	auto onFrame = [&](Data data) {
		if (i < timesOut.size()) {
			ASSERT(data->getMediaTime() == timesOut[i]);
		}
		i++;
	};
	auto demux = create<T>("random_ts.mp4");
	Connect(demux->getOutput(0)->getSignal(), onFrame);
	demux->process(nullptr);
	ASSERT(i == timesOut.size());
}

unittest("encoder: timestamps start at random values (LibavDemux)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { interval, 2 * interval, 3 * interval };
	const std::vector<int64_t> incorrect = { 0 };
	checkTimestamps<Demux::LibavDemux>(correct, incorrect);
}

unittest("encoder: timestamps start at random values (GPACDemuxMP4Simple)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { interval, 2 * interval, 3 * interval };
	checkTimestamps<Demux::GPACDemuxMP4Simple>(correct, correct);
}

unittest("encoder: timestamps start at a negative value (LibavDemux)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { -interval, 0, interval };
	const std::vector<int64_t> incorrect = { 0 };
	checkTimestamps<Demux::LibavDemux>(correct, incorrect);
}

unittest("encoder: timestamps start at a negative value (GPACDemuxMP4Simple)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { -interval, 0, interval };
	checkTimestamps<Demux::GPACDemuxMP4Simple>(correct, correct);
}

#ifdef ENABLE_FAILING_TESTS
unittest("GPAC mp4 mux: don't create empty fragments") {
	auto const segmentDurationInMs = 1000;
	const std::vector<uint64_t> times = { Clock::Rate, 0, 3*Clock::Rate };
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
