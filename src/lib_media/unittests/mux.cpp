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
	o << "filename='" << meta.filename << "'" << std::endl;
	o << "mimeType='" << meta.mimeType << "'" << std::endl;
	o << "codecName='" << meta.codecName << "'" << std::endl;
	o << "durationIn180k=" << meta.durationIn180k << std::endl;
	o << "filesize=" << meta.filesize << std::endl;
	o << "latencyIn180k=" << meta.latencyIn180k << std::endl;
	o << "startsWithRAP=" << meta.startsWithRAP << std::endl;
	o << "eos=" << meta.eos << std::endl;
	return o;
}

unittest("[DISABLED] remux test: GPAC mp4 mux") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto mux = create<Mux::GPACMuxMP4>("out/output_video_gpac");
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
	}

	demux->process(nullptr);
}

//ffmpeg extradata seems to be different (non annex B ?) when output from the muxer
unittest("[DISABLED] remux test: libav mp4 mux") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	auto mux = create<Mux::LibavMux>("out/output_libav", "mp4");
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		ConnectModules(demux.get(), i, mux.get(), i);
	}

	demux->process(nullptr);
}

unittest("mux GPAC mp4 failure tests") {
	const uint64_t segmentDurationInMs = 2000;
	ScopedLogLevel lev(Quiet);

	ASSERT_THROWN(create<Mux::GPACMuxMP4>("out/output_video_gpac_00", segmentDurationInMs, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("out/output_video_gpac_02", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerSegment));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::FlushFragMemory));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("out/output_video_gpac_10", 0, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero));
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero | Mux::GPACMuxMP4::FlushFragMemory););
	ASSERT_THROWN(create<Mux::GPACMuxMP4>("out/output_video_gpac_20", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero););
}

std::vector<Meta> runMux(std::shared_ptr<IModule> m) {
	IOutput* audioPin = nullptr;

	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");

	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto pin = demux->getOutput(i);
		auto metadata = pin->getMetadata();
		if (metadata->isAudio()) {
			audioPin = pin;
		}
	}

	assert(audioPin);

	ConnectOutputToInput(audioPin, m->getInput(0));
	auto listener = create<Listener>(0);
	ConnectModules(m.get(), 0, listener.get(), 0);

	demux->process(nullptr);
	m->flush();

	return std::vector<Meta>(listener->results.begin(), listener->results.end());
}

unittest("mux GPAC mp4: no segment, no fragment") {
	std::vector<Meta> ref = { { 0, "out/output_video_gpac_01.mp4", "audio/mp4", "mp4a.40.2", 0, 10437, 0, 1, 1 } };
	ASSERT_EQUALS(ref, runMux(create<Mux::GPACMuxMP4>("out/output_video_gpac_01", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment)));
}

unittest("mux GPAC mp4: no segment, one fragment per RAP") {
	std::vector<Meta> ref = { { 1, "out/output_video_gpac_03.mp4", "audio/mp4", "mp4a.40.2", 0, 29869, 0, 1, 1 } };
	ASSERT_EQUALS(ref, runMux(create<Mux::GPACMuxMP4>("out/output_video_gpac_03", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerRAP)));
}

unittest("mux GPAC mp4: independent segment, no fragments") {
	std::vector<Meta> ref = { { 2, "out/output_video_gpac_04.mp4", "audio/mp4", "mp4a.40.2", 0, 29869, 4180, 1, 1 } };
	ASSERT_EQUALS(ref, runMux(create<Mux::GPACMuxMP4>("out/output_video_gpac_04", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::OneFragmentPerFrame)));
}

unittest("mux GPAC mp4: independent segment, no fragments (another)") {
	std::vector<Meta> ref = {
		{ 4, "", "audio/mp4", "mp4a.40.2", 363629, 5226, 360000, 1, 1 },
		{ 4, "", "audio/mp4", "mp4a.40.2", 359445, 5336, 359445, 1, 1 },
		{ 4, "", "audio/mp4", "mp4a.40.2", 175543, 3022, 175543, 1, 1 },
	};

	const uint64_t segmentDurationInMs = 2000;
	ASSERT_EQUALS(ref, runMux(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero)));
}

unittest("mux GPAC mp4: fragmented segments, one fragment per segment") {
	std::vector<Meta> ref = {
		{ 9, "", "audio/mp4", "mp4a.40.2", 0, 0, 0, 1, 1 },
		{ 9, "", "audio/mp4", "mp4a.40.2", 363629, 4957, 360000, 1, 1 },
		{ 9, "", "audio/mp4", "mp4a.40.2", 359445, 5047, 359445, 1, 1 },
		{ 9, "", "audio/mp4", "mp4a.40.2", 175543, 2597, 175543, 1, 1 },
	};

	const uint64_t segmentDurationInMs = 2000;
	ASSERT_EQUALS(ref, runMux(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero)));
}

unittest("mux GPAC mp4: fragmented segments, one fragment per RAP") {
	std::vector<Meta> ref = {
		{ 10, "", "audio/mp4", "mp4a.40.2", 0, 0, 0, 1, 1 },
		{ 10, "", "audio/mp4", "mp4a.40.2", 363629, 15629, 360000, 1, 1 },
		{ 10, "", "audio/mp4", "mp4a.40.2", 359445, 15599, 359445, 1, 1 },
		{ 10, "", "audio/mp4", "mp4a.40.2", 175543, 7685, 175543, 1, 1 },
	};

	const uint64_t segmentDurationInMs = 2000;
	ASSERT_EQUALS(ref, runMux(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::SegNumStartsAtZero)));
}

unittest("mux GPAC mp4: fragmented segments, one fragment per frame") {
	std::vector<Meta> ref = {
		{ 11, "", "audio/mp4", "mp4a.40.2", 0, 0, 4180, 1, 1 },
		{ 11, "", "audio/mp4", "mp4a.40.2", 363629, 15629, 4180, 1, 1 },
		{ 11, "", "audio/mp4", "mp4a.40.2", 359445, 15599, 4180, 1, 1 },
		{ 11, "", "audio/mp4", "mp4a.40.2", 175543, 7685, 4180, 1, 1 },
	};

	const uint64_t segmentDurationInMs = 2000;
	ASSERT_EQUALS(ref, runMux(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerFrame, Mux::GPACMuxMP4::SegNumStartsAtZero)));
}

// remove this when the below tests are split
void operator+=(std::vector<Meta>& dst, std::vector<Meta> const& src) {
	for(auto& val : src)
		dst.push_back(val);
}

// causes valgrind errors and GPAC warnings
secondclasstest("mux GPAC mp4 combination coverage: ugly") {
	std::vector<Meta> results, ref = {
		{ 3, "", "audio/mp4", "mp4a.40.2", 0, 10437, 0, 1, 1 },
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

	const uint64_t segmentDurationInMs = 2000;

	results += runMux(create<Mux::GPACMuxMP4>("", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment)); // causes gpac warning: "[BS] Attempt to write on unassigned bitstream"
	results += runMux(create<Mux::GPACMuxMP4>("output_video_gpac_12", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero)); // valgrind reports writes of uninitialized bytes
	results += runMux(create<Mux::GPACMuxMP4>("output_video_gpac_13", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerRAP, Mux::GPACMuxMP4::SegNumStartsAtZero)); // valgrind reports writes of uninitialized bytes
	results += runMux(create<Mux::GPACMuxMP4>("output_video_gpac_14", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::OneFragmentPerFrame, Mux::GPACMuxMP4::SegNumStartsAtZero)); // valgrind reports writes of uninitialized bytes
	results += runMux(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::IndependentSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::SegNumStartsAtZero)); // causes gpac warning: "[BS] Attempt to write on unassigned bitstream"
	results += runMux(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero));// causes gpac warning: "[BS] Attempt to write on unassigned bitstream"
	results += runMux(create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, Mux::GPACMuxMP4::SegNumStartsAtZero | Mux::GPACMuxMP4::FlushFragMemory));// causes gpac warning: "[BS] Attempt to write on unassigned bitstream"

	ASSERT_EQUALS(ref, results);
}

}
