#include "tests/tests.hpp"
#include "lib_media/decode/decoder.hpp"
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

class DummySource : public Module {
	public:
		DummySource() {
			output = addOutput<OutputDefault>();
		}
		void process() {
			output->emit(output->getBuffer(1));
		}
		OutputDefault* output;
};

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
		p.waitForEndOfStream();
	}
	{
		Pipeline p;
		p.start();
		p.waitForEndOfStream();
	}
}

unittest("pipeline: connect inputs to outputs") {
	Pipeline p;
	auto demux = p.addModule<DummySource>();
	auto null = p.addModule<Out::Null>();
	ASSERT_THROWN(p.connect(null, 0, demux, 0));
}

unittest("pipeline: connect incompatible i/o") {
	Pipeline p(false, Pipeline::Mono);
	auto demux = p.addModule<DummySource>();
	PcmFormat fmt;
	auto aconv = p.addModule<Transform::AudioConvert>(fmt, fmt);
	p.connect(demux, 0, aconv, 0);
	ASSERT_THROWN(p.start());
}

unittest("pipeline: longer pipeline") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
		auto metadata = safe_cast<const MetadataPkt>(demux->getOutput(i)->getMetadata());
		auto decode = p.addModule<Decode::Decoder>(metadata->getStreamType());
		p.connect(demux, i, decode, 0);
		auto null = p.addModule<Out::Null>();
		p.connect(decode, 0, null, 0);
	}

	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: longer pipeline with join") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	auto null = p.addModule<Out::Null>();
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
		auto metadata = safe_cast<const MetadataPkt>(demux->getOutput(i)->getMetadata());
		auto decode = p.addModule<Decode::Decoder>(metadata->getStreamType());
		p.connect(demux, i, decode, 0);
		p.connect(decode, 0, null, 0, true);
	}

	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: input data is manually queued while module is running") {
	Pipeline p;
	auto demux = p.addModule<DummySource>();
	auto dualInput = p.addModule<DualInput>();
	p.connect(demux, 0, dualInput, 0);
	p.start();
	auto data = make_shared<DataRaw>(0);
	dualInput->getInput(1)->push(data);
	dualInput->getInput(1)->process();
	p.waitForEndOfStream();
}

unittest("pipeline: multiple inputs (send same packets to 2 inputs and check call number)") {
	Pipeline p;
	auto generator = p.addModule<DummySource>();
	auto dualInput = p.addModule<ThreadedDualInput>();
	p.connect(generator, 0, dualInput, 0);
	p.connect(generator, 0, dualInput, 1);
	p.start();
	p.waitForEndOfStream();
	ASSERT_EQUALS(ThreadedDualInput::numCalls, 1u);
}

}

uint64_t ThreadedDualInput::numCalls = 0;
