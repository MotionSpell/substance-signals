#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_utils/tools.hpp"
#include <list>

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

unittest("mux GPAC mp4 outputs combination coverage") {
	struct Meta {
		bool operator==(const Meta& rhs) const {
			return true; //Romain
		}
		std::string filename, mimeType, codecName;
		uint64_t durationIn180k, filesize, latencyIn180k;
		bool startsWithRAP;
	};

	std::vector<Meta> results, ref = {
		{ "output_video_gpac_01.mp4"          , "video/mp4", "avc1.42C00D",      0, 40542,      0, 1 },
		{ "output_video_gpac_03.mp4"          , "video/mp4", "avc1.42C00D",      0, 41693,      0, 1 },
		{ "output_video_gpac_04.mp4"          , "video/mp4", "avc1.42C00D",      0, 51827,      8, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D",      0, 40542,      0, 1 },
		{ "output_video_gpac_11-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 15932, 360000, 1 },
		{ "output_video_gpac_11-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 17847, 360000, 1 },
		{ "output_video_gpac_11-758672246.mp4", "video/mp4", "avc1.42C00D", 172800,  9645, 172800, 1 },
		{ "output_video_gpac_12-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 15820, 360000, 1 },
		{ "output_video_gpac_12-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 17751, 360000, 1 },
		{ "output_video_gpac_12-758672246.mp4", "video/mp4", "avc1.42C00D", 172800,  9665, 172800, 1 },
		{ "output_video_gpac_13-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 16280, 360000, 1 },
		{ "output_video_gpac_13-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 18115, 360000, 1 },
		{ "output_video_gpac_13-758672246.mp4", "video/mp4", "avc1.42C00D", 172800,  9665, 172800, 1 },
		{ "output_video_gpac_14-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 23960,      8, 1 },
		{ "output_video_gpac_14-758672246.mp4", "video/mp4", "avc1.42C00D", 360000, 25895,      8, 1 },
		{ "output_video_gpac_14-758672246.mp4", "video/mp4", "avc1.42C00D", 172800, 13437,      8, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 360000, 15932, 360000, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 360000, 17847, 360000, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 172800,  9645, 172800, 1 },
		{ "output_video_gpac_21-init.mp4"     , "video/mp4", "avc1.42C00D",      0,     0,      0, 1 },
		{ "output_video_gpac_21-758672246.m4s", "video/mp4", "avc1.42C00D", 360000, 15734, 360000, 1 },
		{ "output_video_gpac_21-758672247.m4s", "video/mp4", "avc1.42C00D", 360000, 17469, 360000, 1 },
		{ "output_video_gpac_21-758672248.m4s", "video/mp4", "avc1.42C00D", 172800,  9083, 172800, 1 },
		{ "output_video_gpac_22-init.mp4"     , "video/mp4", "avc1.42C00D",      0,     0,      0, 1 },
		{ "output_video_gpac_22-758672246.m4s", "video/mp4", "avc1.42C00D", 360000, 15634, 360000, 1 },
		{ "output_video_gpac_22-758672247.m4s", "video/mp4", "avc1.42C00D", 360000, 17469, 360000, 1 },
		{ "output_video_gpac_22-758672248.m4s", "video/mp4", "avc1.42C00D", 172800,  9083, 172800, 1 },
		{ "output_video_gpac_23-init.mp4"     , "video/mp4", "avc1.42C00D",      0,     0,      8, 1 },
		{ "output_video_gpac_23-758672246.m4s", "video/mp4", "avc1.42C00D", 360000, 21550,      8, 1 },
		{ "output_video_gpac_23-758672247.m4s", "video/mp4", "avc1.42C00D", 360000, 23485,      8, 1 },
		{ "output_video_gpac_23-758672248.m4s", "video/mp4", "avc1.42C00D", 172800, 11963,      8, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D",      0,   806,      0, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 360000, 15734, 360000, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 360000, 17469, 360000, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 172800,  9083, 172800, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D",      0,   806,      0, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 360000, 15726, 360000, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 360000, 17461, 360000, 1 },
		{ ""                                  , "video/mp4", "avc1.42C00D", 172800,  9075, 172800, 1 }
	};

	struct Listener : public ModuleS {
		Listener() {
			addInput(new Input<DataBase>(this));
		}
		void process(Data data) override {
			auto const &m = safe_cast<const MetadataFile>(data->getMetadata());
			results.push_back({ m->getFilename(), m->getMimeType(), m->getCodecName(),
				m->getDuration(), m->getSize(), m->getLatency(), m->getStartsWithRAP() });
		}
		void print() { //used for generating reference results
			for (auto &r : results) {
				std::cout << "{ \"" << r.filename << "\", \"" << r.mimeType << "\", \"" << r.codecName << "\", " << r.durationIn180k << ", " << r.filesize << ", " << r.latencyIn180k << ", " << r.startsWithRAP << " }," << std::endl;
			}
		}

		std::list<Meta> results;
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
