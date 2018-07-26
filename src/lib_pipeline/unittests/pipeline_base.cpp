#include "tests/tests.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "pipeline_common.hpp"

using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {

class DataCustom : public DataRaw {};

struct CustomDataTypeSink : public Modules::ModuleS {
	CustomDataTypeSink() {
		addInput(new Input<DataCustom>(this));
	}
	void process(Modules::Data data) override {
		safe_cast<const DataCustom>(data);
	}
};

struct Split : public Modules::ModuleS {
	Split() {
		addOutput<Modules::OutputDefault>();
		addOutput<Modules::OutputDefault>();
	}
	void process(Modules::Data) override {
	}
};

}

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
	auto p = std::make_unique<Pipeline>();
	auto src = p->addModule<InfiniteSource>();
	auto sink = p->addModule<FakeSink>();
	ASSERT_THROWN(p->connect(sink, 0, src, 0));
}

unittest("pipeline: connect incompatible i/o") {
	Pipeline p(false, Pipeline::Mono);
	auto src = p.addModule<InfiniteSource>();
	auto aconv = p.addModule<CustomDataTypeSink>();
	p.connect(src, 0, aconv, 0);
	ASSERT_THROWN(p.start());
}

unittest("pipeline: pipeline with split (no join)") {
	Pipeline p;
	auto src = p.addModule<Split>();
	ASSERT(src->getNumOutputs() >= 2);
	for (int i = 0; i < (int)src->getNumOutputs(); ++i) {
		auto passthru = p.addModule<Passthru>();
		p.connect(src, i, passthru, 0);
		auto sink = p.addModule<FakeSink>();
		p.connect(passthru, 0, sink, 0);
	}

	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: pipeline with split (join)") {
	Pipeline p;
	auto src = p.addModule<Split>();
	ASSERT(src->getNumOutputs() >= 2);
	auto sink = p.addModule<FakeSink>();
	for (int i = 0; i < (int)src->getNumOutputs(); ++i) {
		auto passthru = p.addModule<Passthru>();
		p.connect(src, i, passthru, 0);
		p.connect(passthru, 0, sink, 0, true);
	}

	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: input data is manually queued while module is running") {
	Pipeline p;
	auto src = p.addModule<FakeSource>();
	auto dualInput = p.addModule<DualInput>();
	p.connect(src, 0, dualInput, 0);
	p.start();
	auto data = make_shared<DataRaw>(0);
	dualInput->getInput(1)->push(data);
	dualInput->getInput(1)->process();
	p.waitForEndOfStream();
}

unittest("pipeline: multiple inputs (send same packets to 2 inputs and check call number)") {
	Pipeline p;
	auto generator = p.addModule<FakeSource>(1);
	auto dualInput = p.addModule<ThreadedDualInput>();
	p.connect(generator, 0, dualInput, 0);
	p.connect(generator, 0, dualInput, 1);
	p.start();
	p.waitForEndOfStream();
	ASSERT_EQUALS(ThreadedDualInput::numCalls, 1u);
}
