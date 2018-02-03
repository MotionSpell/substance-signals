#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_utils/tools.hpp"
#include "modules_common.hpp"

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

unittest("mux GPAC mp4 combination coverage") {
	std::vector<Meta> results, ref = {
	{ "output_video_gpac_01.mp4"     , "audio/mp4", "mp4a.40.2", 0     , 10437, 0     , 1 },
	{ "output_video_gpac_03.mp4"     , "audio/mp4", "mp4a.40.2", 0     , 36741, 0     , 1 },
	{ "output_video_gpac_04.mp4"     , "audio/mp4", "mp4a.40.2", 0     , 29837, 4180  , 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 0     , 10437, 0     , 1 },
	{ "output_video_gpac_11-0.mp4"   , "audio/mp4", "mp4a.40.2", 363633, 5202 , 360000, 1 },
	{ "output_video_gpac_11-1.mp4"   , "audio/mp4", "mp4a.40.2", 359992, 5449 , 359992, 1 },
	{ "output_video_gpac_11-2.mp4"   , "audio/mp4", "mp4a.40.2", 171372, 2853 , 171372, 1 },
	{ "output_video_gpac_12-0.mp4"   , "audio/mp4", "mp4a.40.2", 363633, 4934 , 360000, 1 },
	{ "output_video_gpac_12-1.mp4"   , "audio/mp4", "mp4a.40.2", 359992, 5181 , 359992, 1 },
	{ "output_video_gpac_12-2.mp4"   , "audio/mp4", "mp4a.40.2", 171372, 2785 , 171372, 1 },
	{ "output_video_gpac_13-0.mp4"   , "audio/mp4", "mp4a.40.2", 363633, 27642, 360000, 1 },
	{ "output_video_gpac_13-1.mp4"   , "audio/mp4", "mp4a.40.2", 359992, 27889, 359992, 1 },
	{ "output_video_gpac_13-2.mp4"   , "audio/mp4", "mp4a.40.2", 171372, 13349, 171372, 1 },
	{ "output_video_gpac_14-0.mp4"   , "audio/mp4", "mp4a.40.2", 363633, 19290, 4180  , 1 },
	{ "output_video_gpac_14-1.mp4"   , "audio/mp4", "mp4a.40.2", 359992, 19537, 4180  , 1 },
	{ "output_video_gpac_14-2.mp4"   , "audio/mp4", "mp4a.40.2", 171372, 9413 , 4180  , 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 363633, 5202 , 360000, 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 359992, 5449 , 359992, 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 171372, 2853 , 171372, 1 },
	{ "output_video_gpac_21-init.mp4", "audio/mp4", "mp4a.40.2", 0     , 0    , 0     , 1 },
	{ "output_video_gpac_21-0.m4s"   , "audio/mp4", "mp4a.40.2", 363633, 5025 , 360000, 1 },
	{ "output_video_gpac_21-1.m4s"   , "audio/mp4", "mp4a.40.2", 359992, 5272 , 359992, 1 },
	{ "output_video_gpac_21-2.m4s"   , "audio/mp4", "mp4a.40.2", 171372, 2508 , 171372, 1 },
	{ "output_video_gpac_22-init.mp4", "audio/mp4", "mp4a.40.2", 0     , 0    , 0     , 1 },
	{ "output_video_gpac_22-0.m4s"   , "audio/mp4", "mp4a.40.2", 363633, 21193, 360000, 1 },
	{ "output_video_gpac_22-1.m4s"   , "audio/mp4", "mp4a.40.2", 359992, 21440, 359992, 1 },
	{ "output_video_gpac_22-2.m4s"   , "audio/mp4", "mp4a.40.2", 171372, 10028, 171372, 1 },
	{ "output_video_gpac_23-init.mp4", "audio/mp4", "mp4a.40.2", 0     , 0    , 4180  , 1 },
	{ "output_video_gpac_23-0.m4s"   , "audio/mp4", "mp4a.40.2", 363633, 15625, 4180  , 1 },
	{ "output_video_gpac_23-1.m4s"   , "audio/mp4", "mp4a.40.2", 359992, 15872, 4180  , 1 },
	{ "output_video_gpac_23-2.m4s"   , "audio/mp4", "mp4a.40.2", 171372, 7404 , 4180  , 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 0     , 729  , 0     , 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 363633, 5025 , 360000, 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 359992, 5272 , 359992, 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 171372, 2508 , 171372, 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 0     , 729  , 0     , 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 363633, 5017 , 360000, 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 359992, 5264 , 359992, 1 },
	{ ""                             , "audio/mp4", "mp4a.40.2", 171372, 2500 , 171372, 1 }
	};

	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	std::vector<std::unique_ptr<Mux::GPACMuxMP4>> muxers;
	std::vector<std::unique_ptr<Listener>> listeners;
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

	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("output_video_gpac_10", 0, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero);});
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_11", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero));

	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_12", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_13", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_14", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerFrame, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero | Mux::GPACMuxMP4::FlushFragMemory);});

	CATCH_ERROR([&]() {create<Mux::GPACMuxMP4>("output_video_gpac_20", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero);});
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_21", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_22", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_23", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerFrame, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero | Mux::GPACMuxMP4::FlushFragMemory));

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
		if (metadata->isAudio()) {
			for (auto &m : muxers) {
				ConnectModules(demux.get(), i, m.get(), 0);
				listeners.push_back(create<Listener>());
				ConnectModules(m.get(), 0, listeners.back().get(), 0);
			}
			break;
		}
	}

	demux->process(nullptr);
	for (auto &m : muxers) {
		m->flush();
	}
	for (auto &l : listeners) {
		l->flush();
		for (auto &r : l->results) {
			results.push_back(r);
		}
	}

	ASSERT_EQUALS(results.size(), ref.size());
	ASSERT(std::equal(results.begin(), results.end(), ref.begin()));
}

}
