#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_utils/tools.hpp"
#include <iostream> // cerr
#include <vector>

using namespace Tests;
using namespace Modules;
using namespace std;

auto const VIDEO_RESOLUTION = Resolution(320, 180);

unittest("encoder: video simple") {
	auto picture = make_shared<PictureYUV420P>(VIDEO_RESOLUTION);

	int numEncodedFrames = 0;
	auto onFrame = [&](Data /*data*/) {
		numEncodedFrames++;
	};

	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video);
	ConnectOutput(encode.get(), onFrame);
	for (int i = 0; i < 37; ++i) {
		picture->setMediaTime(i); // avoid warning about non-monotonic pts
		encode->process(picture);
	}
	encode->flush();

	ASSERT_EQUALS(37, numEncodedFrames);
}

unittest("encoder: audio with first negative timestamp") {
	vector<int64_t> times;
	PcmFormat fmt;
	auto const numSamplesPerFrame = 1024LL;
	const int64_t inFrameSizeInBytes = numSamplesPerFrame * fmt.getBytesPerSample() / fmt.numPlanes;

	auto onFrame = [&](Data data) {
		times.push_back(data->getMediaTime());
	};

	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Audio);
	ConnectOutput(encode.get(), onFrame);
	for (int i = 0; i < 4; ++i) {
		auto pcm = make_shared<DataPcm>(0);
		pcm->setFormat(fmt);
		std::vector<uint8_t> input((size_t)inFrameSizeInBytes);
		auto inputRaw = input.data();
		for (uint8_t i = 0; i < fmt.numPlanes; ++i) {
			pcm->setPlane(i, inputRaw, inFrameSizeInBytes);
		}
		pcm->setMediaTime(i * timescaleToClock(numSamplesPerFrame, fmt.sampleRate));
		encode->process(pcm);
	}
	encode->flush();

	vector<int64_t> expected = { -4180, 0, 4180, 8360, 12540 };
	ASSERT_EQUALS(expected, times);
}

unittest("encoder: timestamp passthrough") {
	vector<int64_t> times;
	auto onFrame = [&](Data data) {
		times.push_back(data->getMediaTime());
	};

	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video);
	ConnectOutput(encode.get(), onFrame);
	for (int i = 0; i < 5; ++i) {
		auto picture = make_shared<PictureYUV420P>(VIDEO_RESOLUTION);
		picture->setMediaTime(i);
		encode->process(picture);
	}
	encode->flush();

	vector<int64_t> expected = {0, 1, 2, 3, 4};
	ASSERT_EQUALS(expected, times);
}

secondclasstest("H265 encode and GPAC mp4 mux") {
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	p.avcodecCustom = "-vcodec libx265";
	try {
		auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
		auto mux = create<Mux::GPACMuxMP4>(Mp4MuxConfig{"tmp"});
		ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));

		auto picture = make_shared<PictureYUV420P>(VIDEO_RESOLUTION);
		encode->process(picture);
		encode->flush();
		mux->flush();
	} catch (exception const &e) {
		cerr << "No support for \"" << p.avcodecCustom << "\". Skipping test (" << e.what() << ")" << endl;
	}
}

void RAPTest(const Fraction fps, const vector<uint64_t> &times, const vector<bool> &RAPs) {
	Encode::LibavEncode::Params p;
	p.frameRate = fps;
	p.GOPSize = fps;
	auto picture = make_shared<PictureYUV420P>(VIDEO_RESOLUTION);
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	size_t i = 0;
	auto onFrame = [&](Data data) {
		if (i < RAPs.size()) {
			auto pkt = safe_cast<const DataAVPacket>(data);
			ASSERT(pkt->isRap() == RAPs[i]);
		}
		i++;
	};
	ConnectOutput(encode.get(), onFrame);
	for (size_t i = 0; i < times.size(); ++i) {
		picture->setMediaTime(times[i]);
		encode->process(picture);
	}
	encode->flush();
	ASSERT(i == RAPs.size());
}

unittest("encoder: RAP placement (25/1 fps)") {
	const vector<uint64_t> times = { 0, IClock::Rate / 2, IClock::Rate, IClock::Rate * 3 / 2, IClock::Rate * 2 };
	const vector<bool> RAPs = { true, false, true, false, true };
	RAPTest(Fraction(25, 1), times, RAPs);
}

unittest("encoder: RAP placement (30000/1001 fps)") {
	const vector<uint64_t> times = { 0, IClock::Rate/2, IClock::Rate, IClock::Rate*3/2, IClock::Rate*2 };
	const vector<bool> RAPs = { true, false, true, false, true };
	RAPTest(Fraction(30000, 1001), times, RAPs);
}

unittest("encoder: RAP placement (noisy)") {
	const auto &ms = std::bind(timescaleToClock<uint64_t>, std::placeholders::_1, 1000);
	const vector<uint64_t> times = { 0, ms(330), ms(660), ms(990), ms(1330), ms(1660) };
	const vector<bool> RAPs = { true, false, false, true, false, false };
	RAPTest(Fraction(3, 1), times, RAPs);
}

unittest("encoder: RAP placement (incorrect timings)") {
	const vector<uint64_t> times = { 0, 0, IClock::Rate };
	const vector<bool> RAPs = { true, false, true };
	RAPTest(Fraction(25, 1), times, RAPs);
}

unittest("GPAC mp4 mux: don't create empty fragments") {
	struct Recorder : ModuleS {
		Recorder() {
			addInput(new Input<DataBase>(this));
		}
		void process(Data data) {
			if (initFound)
				ASSERT(safe_cast<const MetadataFile>(data->getMetadata())->durationIn180k > 0);
			initFound = true;
		}
		bool initFound = false;
	};
	auto const segmentDurationInMs = 1000;
	const vector<uint64_t> times = { IClock::Rate, 0, 3 * IClock::Rate, (7 * IClock::Rate) / 2, 4 * IClock::Rate };
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	auto picture = make_shared<PictureYUV420P>(VIDEO_RESOLUTION);
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	auto mux = create<Mux::GPACMuxMP4>(Mp4MuxConfig{"", segmentDurationInMs, FragmentedSegment, OneFragmentPerRAP, Browsers | SegmentAtAny});
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));
	auto recorder = create<Recorder>();
	ConnectOutputToInput(mux->getOutput(0), recorder->getInput(0));
	for (size_t i = 0; i < times.size(); ++i) {
		picture->setMediaTime(times[i]);
		encode->process(picture);
	}
	encode->flush();
	mux->flush();
}

//TODO: add a more complex test for each module!
