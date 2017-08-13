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
	const uint64_t segmentDurationInMs = 2000;

	bool thrown = false;
	try {
		auto mux01 = create<Mux::GPACMuxMP4>("output_video_gpac_01", segmentDurationInMs, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);

	auto mux00 = create<Mux::GPACMuxMP4>("output_video_gpac_00", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment);
	auto mux02 = create<Mux::GPACMuxMP4>("output_video_gpac_02", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerSegment);
	auto mux04 = create<Mux::GPACMuxMP4>("output_video_gpac_04", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerRAP);
	auto mux06 = create<Mux::GPACMuxMP4>("output_video_gpac_06", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerFrame);

	thrown = false;
	try {
		auto mux10 = create<Mux::GPACMuxMP4>("output_video_gpac_10", 0, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);

	auto mux11 = create<Mux::GPACMuxMP4>("output_video_gpac_11", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment);
	auto mux13 = create<Mux::GPACMuxMP4>("output_video_gpac_13", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerSegment);
	auto mux15 = create<Mux::GPACMuxMP4>("output_video_gpac_15", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerRAP);
	auto mux17 = create<Mux::GPACMuxMP4>("output_video_gpac_17", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerFrame);

	thrown = false;
	try {
		auto mux21 = create<Mux::GPACMuxMP4>("output_video_gpac_21", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::NoFragment);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);

	auto mux23 = create<Mux::GPACMuxMP4>("output_video_gpac_23", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment);
	auto mux25 = create<Mux::GPACMuxMP4>("output_video_gpac_25", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP);
	auto mux27 = create<Mux::GPACMuxMP4>("output_video_gpac_27", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerFrame);

#ifdef ENABLE_FAILING_TESTS
	auto mux31 = create<Mux::GPACMuxMP4>("output_video_gpac_31", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::NoFragment);
	auto mux33 = create<Mux::GPACMuxMP4>("output_video_gpac_33", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerSegment);
	auto mux35 = create<Mux::GPACMuxMP4>("output_video_gpac_35", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerRAP);
	auto mux37 = create<Mux::GPACMuxMP4>("output_video_gpac_37", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerFrame);
#endif

	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = demux->getOutput(i)->getMetadata();
		if (metadata->isVideo()) {
			ConnectModules(demux.get(), i, mux00.get(), 0);
			ConnectModules(demux.get(), i, mux02.get(), 0);
			ConnectModules(demux.get(), i, mux04.get(), 0);
			ConnectModules(demux.get(), i, mux06.get(), 0);

			ConnectModules(demux.get(), i, mux11.get(), 0);
			ConnectModules(demux.get(), i, mux13.get(), 0);
			ConnectModules(demux.get(), i, mux15.get(), 0);
			ConnectModules(demux.get(), i, mux17.get(), 0);

			ConnectModules(demux.get(), i, mux23.get(), 0);
			ConnectModules(demux.get(), i, mux25.get(), 0);
			ConnectModules(demux.get(), i, mux27.get(), 0);

#ifdef ENABLE_FAILING_TESTS
			ConnectModules(demux.get(), i, mux31.get(), 0);
			ConnectModules(demux.get(), i, mux33.get(), 0);
			ConnectModules(demux.get(), i, mux35.get(), 0);
			ConnectModules(demux.get(), i, mux37.get(), 0);
#endif

			break;
		}
	}

	demux->process(nullptr);

	mux00->flush();
	mux02->flush();
	mux04->flush();
	mux06->flush();
	mux11->flush();
	mux13->flush();
	mux15->flush();
	mux17->flush();
	mux23->flush();
	mux25->flush();
	mux27->flush();
#ifdef ENABLE_FAILING_TESTS
	mux31->flush();
	mux33->flush();
	mux35->flush();
	mux37->flush();
#endif
}

}
