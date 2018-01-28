#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

#ifdef ENABLE_FAILING_TESTS
unittest("remux test: GPAC mp4 mux") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto mux = create<Mux::GPACMuxMP4>("output_video_gpac");
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
	}

	demux->process(nullptr);
}

//ffmpeg extradata seems to be different (non annex B ?) when output from the muxer
unittest("remux test: libav mp4 mux") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto mux = create<Mux::LibavMux>("output_libav", "mp4");
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
	}

	demux->process(nullptr);
}

unittest("multiple inputs: send same packets to 2 GPAC mp4 mux inputs") {
	assert(0);//TODO
}
#endif /*ENABLE_FAILING_TESTS*/

unittest("GPAC mp4 mux outputs combination coverage") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	std::vector<std::unique_ptr<Mux::GPACMuxMP4>> muxers;
	const uint64_t segmentDurationInMs = 2000;
	bool thrown = false;
	auto CATCH_ERROR = [&](std::function<void()> creation) {
		thrown = false;
		try {
			creation();
		} catch (std::exception const& e) {
			std::cerr << "Expected error: " << e.what() << std::endl;
			thrown = true;
		}
		ASSERT(thrown);
	};

	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("output_video_gpac_00", segmentDurationInMs, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment);});
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_01", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("output_video_gpac_02", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerSegment);});
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_03", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_04", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
	muxers.push_back(create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::FlushFragMemory);});

	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("output_video_gpac_10", 0, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment);});
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_11", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_12", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_13", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_14", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment));
	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::FlushFragMemory);});

	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("output_video_gpac_20", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::NoFragment);});
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_21", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_22", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_23", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::FlushFragMemory));

#ifdef ENABLE_FAILING_TESTS
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_31", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::NoFragment));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_32", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_33", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_34", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
	muxers.push_back(create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::NoFragment);
	CATCH_ERROR([&]() { create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::FlushFragMemory);
#endif

	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = demux->getOutput(i)->getMetadata();
		if (metadata->isVideo()) {
			for (auto &m : muxers) {
				ConnectModules(demux.get(), i, m.get(), 0);
			}
			break;
		}
	}

	demux->process(nullptr);

	for (auto &m : muxers) {
		m->flush();
	}
}

}
