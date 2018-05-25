#include "tests/tests.hpp"
#include "lib_media/decode/libav_decode.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "pipeline_common.hpp"

using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {

unittest("pipeline: empty") {
	{
		Pipeline p;
	}
	{
		Pipeline p;
		p.start();
	}
	{
		Pipeline p;
		p.waitForCompletion();
	}
	{
		Pipeline p;
		p.start();
		p.waitForCompletion();
	}
}

unittest("pipeline: connect inputs to outputs") {
	bool thrown = false;
	try {
		Pipeline p;
		auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		auto null = p.addModule<Out::Null>();
		for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
			p.connect(null, i, demux, i);
		}
		p.start();
		p.waitForCompletion();
	} catch (std::runtime_error const& /*e*/) {
		thrown = true;
	}
	ASSERT(thrown);
}

unittest("pipeline: connect incompatible i/o") {
	Pipeline p(false, 0.0, Pipeline::Mono | Pipeline::RegulationOffFlag);
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	PcmFormat fmt;
	auto aconv = p.addModule<Transform::AudioConvert>(fmt, fmt);
	p.connect(demux, 0, aconv, 0);
	ASSERT_THROWN(p.start());
}

unittest("pipeline: longer pipeline") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
		auto metadata = safe_cast<const MetadataPktLibav>(demux->getOutput(i)->getMetadata());
		auto decode = p.addModule<Decode::LibavDecode>(metadata);
		p.connect(demux, i, decode, 0);
		auto null = p.addModule<Out::Null>();
		p.connect(decode, 0, null, 0);
	}

	p.start();
	p.waitForCompletion();
}

unittest("pipeline: longer pipeline with join") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	auto null = p.addModule<Out::Null>();
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
		auto metadata = safe_cast<const MetadataPktLibav>(demux->getOutput(i)->getMetadata());
		auto decode = p.addModule<Decode::LibavDecode>(metadata);
		p.connect(demux, i, decode, 0);
		p.connect(decode, 0, null, 0, true);
	}

	p.start();
	p.waitForCompletion();
}

unittest("pipeline: input data is manually queued while module is running") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	auto dualInput = p.addModule<DualInput>(false);
	p.connect(demux, 0, dualInput, 0);
	p.start();
	auto data = std::make_shared<DataRaw>(0);
	dualInput->getInput(1)->push(data);
	dualInput->getInput(1)->process();
	p.waitForCompletion();
}

unittest("pipeline: multiple inputs (send same packets to 2 inputs and check call number)") {
	Pipeline p;
	auto generator = p.addModule<In::VideoGenerator>();
	auto dualInput = p.addModule<DualInput>(true);
	p.connect(generator, 0, dualInput, 0);
	p.connect(generator, 0, dualInput, 1);
	p.start();
	p.waitForCompletion();
	ASSERT_EQUALS(DualInput::numCalls, 1u);
}

}

uint64_t DualInput::numCalls = 0;
