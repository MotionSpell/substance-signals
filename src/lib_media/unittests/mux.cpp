#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "../demux/libav_demux.hpp"
#include "../mux/libav_mux.hpp"
#include "../mux/gpac_mux_mp4.hpp"
#include "lib_utils/tools.hpp"
#include "modules_common.hpp"

using namespace Tests;
using namespace Modules;

namespace {

std::ostream& operator<<(std::ostream& o, Meta const& meta) {
	o << "internalTestIdx=" << meta.internalTestIdx << std::endl;
	o << "filename=" << meta.filename << std::endl;
	o << "mimeType=" << meta.mimeType << std::endl;
	o << "codecName=" << meta.codecName << std::endl;
	o << "durationIn180k=" << meta.durationIn180k << std::endl;
	o << "filesize=" << meta.filesize << std::endl;
	o << "latencyIn180k=" << meta.latencyIn180k << std::endl;
	o << "startsWithRAP=" << meta.startsWithRAP << std::endl;
	o << "eos=" << meta.eos << std::endl;
	return o;
}

unittest("[DISABLED] remux test: GPAC mp4 mux") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto mux = create<Mux::GPACMuxMP4>("output_video_gpac");
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
	}

	demux->process(nullptr);
}

//ffmpeg extradata seems to be different (non annex B ?) when output from the muxer
unittest("[DISABLED] remux test: libav mp4 mux") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto mux = create<Mux::LibavMux>("output_libav", "mp4");
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
	}

	demux->process(nullptr);
}

unittest("mux GPAC mp4 failure tests") {
	const uint64_t segmentDurationInMs = 2000;
	ScopedLogLevel lev(Quiet);

	ASSERT_THROWN(create<Mux::GPACMuxMP4>("output_video_gpac_00", segmentDurationInMs, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("output_video_gpac_02", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::FlushFragMemory));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("output_video_gpac_10", 0, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero | Mux::GPACMuxMP4::FlushFragMemory););
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("output_video_gpac_20", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero););
}

unittest("mux GPAC mp4 combination coverage") {
	std::vector<Meta> results, ref = {
		{ 0, "output_video_gpac_01.mp4", "audio/mp4", "mp4a.40.2", 0, 10437, 0, 1, 1 },
		{ 1, "output_video_gpac_03.mp4", "audio/mp4", "mp4a.40.2", 0, 29869, 0, 1, 1 },
		{ 2, "output_video_gpac_04.mp4", "audio/mp4", "mp4a.40.2", 0, 29869, 4180, 1, 1 },
		{ 3, "", "audio/mp4", "mp4a.40.2", 0, 10437, 0, 1, 1 },
		{ 4, "output_video_gpac_11-0.mp4", "audio/mp4", "mp4a.40.2", 363629, 5226, 360000, 1, 1 },
		{ 4, "output_video_gpac_11-1.mp4", "audio/mp4", "mp4a.40.2", 359445, 5336, 359445, 1, 1 },
		{ 4, "output_video_gpac_11-2.mp4", "audio/mp4", "mp4a.40.2", 175543, 3022, 175543, 1, 1 },
		{ 5, "output_video_gpac_12-0.mp4", "audio/mp4", "mp4a.40.2", 363629, 4838, 360000, 1, 1 },
		{ 5, "output_video_gpac_12-1.mp4", "audio/mp4", "mp4a.40.2", 359445, 4936, 359445, 1, 1 },
		{ 5, "output_video_gpac_12-2.mp4", "audio/mp4", "mp4a.40.2", 175543, 2838, 175543, 1, 1 },
		{ 6, "output_video_gpac_13-0.mp4", "audio/mp4", "mp4a.40.2", 363629, 19298, 360000, 1, 1 },
		{ 6, "output_video_gpac_13-1.mp4", "audio/mp4", "mp4a.40.2", 359445, 19232, 359445, 1, 1 },
		{ 6, "output_video_gpac_13-2.mp4", "audio/mp4", "mp4a.40.2", 175543, 9734, 175543, 1, 1 },
		{ 7, "output_video_gpac_14-0.mp4", "audio/mp4", "mp4a.40.2", 363629, 19298, 4180, 1, 1 },
		{ 7, "output_video_gpac_14-1.mp4", "audio/mp4", "mp4a.40.2", 359445, 19232, 4180, 1, 1 },
		{ 7, "output_video_gpac_14-2.mp4", "audio/mp4", "mp4a.40.2", 175543, 9734, 4180, 1, 1 },
		{ 8, "", "audio/mp4", "mp4a.40.2", 363629, 5226, 360000, 1, 1 },
		{ 8, "", "audio/mp4", "mp4a.40.2", 359445, 5336, 359445, 1, 1 },
		{ 8, "", "audio/mp4", "mp4a.40.2", 175543, 3022, 175543, 1, 1 },
		{ 9, "output_video_gpac_21-init.mp4", "audio/mp4", "mp4a.40.2", 0, 0, 0, 1, 1 },
		{ 9, "output_video_gpac_21-0.m4s", "audio/mp4", "mp4a.40.2", 363629, 4957, 360000, 1, 1 },
		{ 9, "output_video_gpac_21-1.m4s", "audio/mp4", "mp4a.40.2", 359445, 5047, 359445, 1, 1 },
		{ 9, "output_video_gpac_21-2.m4s", "audio/mp4", "mp4a.40.2", 175543, 2597, 175543, 1, 1 },
		{ 10, "output_video_gpac_22-init.mp4", "audio/mp4", "mp4a.40.2", 0, 0, 0, 1, 1 },
		{ 10, "output_video_gpac_22-0.m4s", "audio/mp4", "mp4a.40.2", 363629, 15629, 360000, 1, 1 },
		{ 10, "output_video_gpac_22-1.m4s", "audio/mp4", "mp4a.40.2", 359445, 15599, 359445, 1, 1 },
		{ 10, "output_video_gpac_22-2.m4s", "audio/mp4", "mp4a.40.2", 175543, 7685, 175543, 1, 1 },
		{ 11, "output_video_gpac_23-init.mp4", "audio/mp4", "mp4a.40.2", 0, 0, 4180, 1, 1 },
		{ 11, "output_video_gpac_23-0.m4s", "audio/mp4", "mp4a.40.2", 363629, 15629, 4180, 1, 1 },
		{ 11, "output_video_gpac_23-1.m4s", "audio/mp4", "mp4a.40.2", 359445, 15599, 4180, 1, 1 },
		{ 11, "output_video_gpac_23-2.m4s", "audio/mp4", "mp4a.40.2", 175543, 7685, 4180, 1, 1 },
		{ 12, "", "audio/mp4", "mp4a.40.2", 0, 729, 0, 1, 1 },
		{ 12, "", "audio/mp4", "mp4a.40.2", 363629, 4957, 360000, 1, 1 },
		{ 12, "", "audio/mp4", "mp4a.40.2", 359445, 5047, 359445, 1, 1 },
		{ 12, "", "audio/mp4", "mp4a.40.2", 175543, 2597, 175543, 1, 1 },
		{ 13, "", "audio/mp4", "mp4a.40.2", 0, 729, 0, 1, 1 },
		{ 13, "", "audio/mp4", "mp4a.40.2", 363629, 4949, 360000, 1, 0 },
		{ 13, "", "audio/mp4", "mp4a.40.2", 0, 8, 0, 1, 1 },
		{ 13, "", "audio/mp4", "mp4a.40.2", 359445, 5039, 359445, 1, 0 },
		{ 13, "", "audio/mp4", "mp4a.40.2", 0, 8, 0, 1, 1 },
		{ 13, "", "audio/mp4", "mp4a.40.2", 175543, 2589, 175543, 1, 0 },
		{ 13, "", "audio/mp4", "mp4a.40.2", 0, 8, 0, 1, 1 },
	};

	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	std::vector<std::unique_ptr<Mux::GPACMuxMP4>> muxers;
	const uint64_t segmentDurationInMs = 2000;

	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_01", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_03", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerRAP));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_04", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerFrame));
	muxers.push_back(create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_11", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_12", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_13", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_14", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerFrame, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_21", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_22", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("output_video_gpac_23", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerFrame, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	muxers.push_back(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero | Mux::GPACMuxMP4::FlushFragMemory));

	std::vector<std::unique_ptr<Listener>> listeners;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = demux->getOutput(i)->getMetadata();
		if (metadata->isAudio()) {
			for (auto &m : muxers) {
				ConnectModules(demux.get(), i, m.get(), 0);
				listeners.push_back(create<Listener>(listeners.size()));
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
		for (auto &r : l->results) results.push_back(r);
	}

	ASSERT_EQUALS(ref, results);
}

}
