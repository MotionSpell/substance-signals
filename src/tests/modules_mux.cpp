#include "tests.hpp"
#include "lib_modules/modules.hpp"

#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_utils/tools.hpp"


using namespace Tests;
using namespace Modules;

namespace {

unittest("remux test: GPAC mp4 mux") {
	auto demux = uptr(create<Demux::LibavDemux>("data/beepbop.mp4"));
	auto mux = uptr(create<Mux::GPACMuxMP4>("output_video_gpac"));
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
		break; //FIXME
	}

	demux->process(nullptr);
}

#ifdef ENABLE_FAILING_TESTS
//ffmpeg extradata seems to be different (non annex B ?) when output from the muxer
unittest("remux test: libav mp4 mux") {
	auto demux = uptr(create<Demux::LibavDemux>("data/beepbop.mp4"));
	auto mux = uptr(create<Mux::LibavMux>("output_video_libav", "mp4"));
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
		break; //FIXME
	}

	demux->process(nullptr);
}
#endif

#ifdef ENABLE_FAILING_TESTS
unittest("multiple inputs: send same packets to 2 GPAC mp4 mux inputs") {
	//TODO
}
#endif

//FIXME: enable all paths
unittest("GPAC mp4 mux outputs combination coverage") {
	auto demux = uptr(create<Demux::LibavDemux>("data/beepbop.mp4"));
	const uint64_t segmentDurationInMs = 2000;

	bool thrown = false;
	try {
		auto mux01 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_01", segmentDurationInMs, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
	}
	catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);

	auto mux00 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_00", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
#if ENABLE_FAILING_TESTS
	auto mux02 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_02", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	auto mux04 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_04", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	auto mux06 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_06", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
#endif

	thrown = false;
	try {
		auto mux10 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_10", 0, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment));
	}
	catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);

	auto mux11 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_11", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment));
#if ENABLE_FAILING_TESTS
	auto mux13 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_13", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	auto mux15 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_15", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	auto mux17 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_17", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
#endif

	thrown = false;
	try {
		auto mux21 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_21", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::NoFragment));
	}
	catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);

	auto mux23 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_23", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	auto mux25 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_25", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	auto mux27 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_27", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));

#if ENABLE_FAILING_TESTS
	auto mux31 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_31", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::NoFragment));
	auto mux33 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_33", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	auto mux35 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_35", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	auto mux37 = uptr(create<Mux::GPACMuxMP4>("output_video_gpac_37", 0, Mux::GPACMuxMP4::SingleSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
#endif

	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = getMetadataFromOutput<IMetadata>(demux->getOutput(i));
		if (metadata->isVideo()) {
			ConnectModules(demux.get(), i, mux00.get(), 0);
#if ENABLE_FAILING_TESTS
			ConnectModules(demux.get(), i, mux02.get(), 0);
			ConnectModules(demux.get(), i, mux04.get(), 0);
			ConnectModules(demux.get(), i, mux06.get(), 0);
#endif

			ConnectModules(demux.get(), i, mux11.get(), 0);
#if ENABLE_FAILING_TESTS
			ConnectModules(demux.get(), i, mux13.get(), 0);
			ConnectModules(demux.get(), i, mux15.get(), 0);
			ConnectModules(demux.get(), i, mux17.get(), 0);
#endif

			ConnectModules(demux.get(), i, mux23.get(), 0);
			ConnectModules(demux.get(), i, mux25.get(), 0);
			ConnectModules(demux.get(), i, mux27.get(), 0);

#if ENABLE_FAILING_TESTS
			ConnectModules(demux.get(), i, mux31.get(), 0);
			ConnectModules(demux.get(), i, mux33.get(), 0);
			ConnectModules(demux.get(), i, mux35.get(), 0);
			ConnectModules(demux.get(), i, mux37.get(), 0);
#endif

			break; //FIXME
		}
	}

	demux->process(nullptr);

	mux00->flush();
#if ENABLE_FAILING_TESTS
	mux02->flush();
	mux04->flush();
	mux06->flush();
#endif
	mux11->flush();
#if ENABLE_FAILING_TESTS
	mux13->flush();
	mux15->flush();
	mux17->flush();
#endif
	mux23->flush();
	mux25->flush();
	mux27->flush();
#if ENABLE_FAILING_TESTS
	mux31->flush();
	mux33->flush();
	mux35->flush();
	mux37->flush();
#endif
}

}
