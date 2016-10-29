#include "tests.hpp"
#include "lib_media/decode/libav_decode.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_modules/utils/pipeline.hpp"


using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {

class DualInput : public Module {
public:
	DualInput(bool threaded) : threaded(threaded) {
		addInput(new Input<DataBase>(this));
		addInput(new Input<DataBase>(this));
		numCallsMutex.lock();
		numCalls = 0;
		if (threaded)
			workingThread = std::thread(&DualInput::threadProc, this);
	}

	virtual ~DualInput() {
		numCallsMutex.unlock();
		if (workingThread.joinable()) {
			for (size_t i = 0; i < inputs.size(); ++i) {
				inputs[i]->push(nullptr);
			}
			workingThread.join();
		}
	}

	void process() {
		if (!threaded) {
			threadProc();
		}
	}

	void threadProc() {
		numCalls++;

		if (!done) {
			auto i1 = getInput(0)->pop();
			auto i2 = getInput(1)->pop();
			done = true;
		}

		getInput(0)->clear();
		getInput(1)->clear();
	}

	static uint64_t numCalls;

private:
	bool done = false, threaded;
	std::thread workingThread;
	std::mutex numCallsMutex;
};
uint64_t DualInput::numCalls = 0;

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

unittest("pipeline: source only") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	p.start();
	p.waitForCompletion();
}

unittest("pipeline: interrupted") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto null = p.addModule<Out::Null>();
	p.connect(demux, 0, null, 0);
	p.start();
	auto f = [&]() {
		p.exitSync();
	};
	std::thread tf(f);
	p.waitForCompletion();
	tf.join();
}

unittest("pipeline: connect while running") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto null1 = p.addModule<Out::Null>();
	auto null2 = p.addModule<Out::Null>();
	p.connect(demux, 0, null1, 0);
	p.start();
	auto f = [&]() {
		p.connect(demux, 0, null2, 0);
	};
	std::thread tf(f);
	p.waitForCompletion();
	tf.join();
}

unittest("pipeline: connect one input (out of 2) to one output") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto null = p.addModule<Out::Null>();
	p.connect(demux, 0, null, 0);
	p.start();
	p.waitForCompletion();
}

unittest("pipeline: connect two outpus to the same output") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto null = p.addModule<Out::Null>();
	p.connect(demux, 0, null, 0);
	p.connect(demux, 1, null, 0, true);
	p.start();
	p.waitForCompletion();
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
	bool thrown = false;
	try {
		Pipeline p(false, 0.0, Pipeline::Mono | Pipeline::RegulationOffFlag);
		auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		PcmFormat fmt;
		auto aconv = p.addModule<Transform::AudioConvert>(fmt, fmt);
		p.connect(demux, 0, aconv, 0);
		p.start();
		p.waitForCompletion();
	} catch (std::runtime_error const& /*e*/) {
		thrown = true;
	}
	ASSERT(thrown);
}

unittest("pipeline: source only") {
	bool thrown = false;
	try {
		Pipeline p;
		p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		p.start();
		p.waitForCompletion();
	} catch (...) {
		thrown = true;
	}
	ASSERT(!thrown);
}

unittest("pipeline: sink only (incorrect topology)") {
	try {
		Pipeline p;
		p.addModule<Out::Null>();
		p.start();
		p.waitForCompletion();
	} catch (...) {
	}
}

unittest("pipeline: dynamic module connection of an existing module") {
	try {
		Pipeline p;
		auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		auto dualInput = p.addModule<DualInput>(false);
		p.connect(demux, 0, dualInput, 0);
		p.start();
		p.connect(demux, 0, dualInput, 1);
		p.waitForCompletion();
	} catch (std::runtime_error const& /*e*/) {
	}
}

unittest("pipeline: dynamic module connection of a new module") {
	try {
		Pipeline p;
		auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		auto dualInput = p.addModule<DualInput>(false);
		p.connect(demux, 0, dualInput, 0);
		auto demux2 = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		p.start();
		p.connect(demux2, 0, dualInput, 1);
		p.waitForCompletion();
	} catch (std::runtime_error const& /*e*/) {
	}
}

unittest("pipeline: dynamic module connection of a new module") {
	try {
		Pipeline p;
		auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		auto dualInput = p.addModule<DualInput>(false);
		p.connect(demux, 0, dualInput, 0);
		p.start();
		auto demux2 = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		p.connect(demux2, 0, dualInput, 1);
		if (demux2->isSource()) demux2->process(); //only sources need to be triggered
		p.waitForCompletion();
	} catch (std::runtime_error const& /*e*/) {
	}
}

unittest("pipeline: input data is manually queued while module is running") {
	try {
		Pipeline p;
		auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		auto dualInput = p.addModule<DualInput>(false);
		p.connect(demux, 0, dualInput, 0);
		p.start();
		auto data = std::make_shared<DataRaw>(0);
		dualInput->getInput(1)->push(data);
		dualInput->getInput(1)->process();
		p.waitForCompletion();
	} catch (std::runtime_error const& /*e*/) {
	}
}

unittest("pipeline: multiple inputs (send same packets to 2 inputs and check call number)") {
	Pipeline p;
	auto generator = p.addModule<In::VideoGenerator>();
	auto dualInput = p.addModule<DualInput>(true);
	p.connect(generator, 0, dualInput, 0);
	p.connect(generator, 0, dualInput, 1);
	p.start();
	p.waitForCompletion();
	ASSERT_EQUALS(DualInput::numCalls, 1);
}

unittest("pipeline: longer pipeline") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
		auto metadata = getMetadataFromOutput<MetadataPktLibav>(demux->getOutput(i));
		auto decode = p.addModule<Decode::LibavDecode>(*metadata);
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
		auto metadata = getMetadataFromOutput<MetadataPktLibav>(demux->getOutput(i));
		auto decode = p.addModule<Decode::LibavDecode>(*metadata);
		p.connect(demux, i, decode, 0);
		p.connect(decode, 0, null, 0, true);
	}

	p.start();
	p.waitForCompletion();
}
}