#include "tests/tests.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "pipeline_common.hpp"

using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {

class DataCustom : public DataRaw {};

struct CustomDataTypeSink : public Modules::ModuleS {
	CustomDataTypeSink(Modules::KHost*) {
		addInput();
	}
	void processOne(Modules::Data data) override {
		safe_cast<const DataCustom>(data);
	}
};

struct Split : public Modules::Module {
	Split(Modules::KHost* host) : host(host) {
		addOutput<Modules::OutputDefault>();
		addOutput<Modules::OutputDefault>();
		host->activate(true);
	}
	void process() override {
		host->activate(false);
	}
	Modules::KHost* host;
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

unittest("pipeline: connecting an input to and output throws an error") {
	auto p = std::make_unique<Pipeline>();
	auto src = p->addNamedModule<InfiniteSource>("Source");
	auto sink = p->addNamedModule<FakeSink>("Sink");
	ASSERT_THROWN(p->connect(sink, src));
}

unittest("pipeline: connecting incompatible i/o throws an error") {
	Pipeline p(nullptr, false, Pipelines::Threading::Mono);
	auto src = p.addModule<InfiniteSource>();
	auto aconv = p.addModule<CustomDataTypeSink>();
	p.connect(src, aconv);
	ASSERT_THROWN(p.start());
}

unittest("pipeline: pipeline with split (no join)") {
	Pipeline p;
	auto src = p.addModule<Split>();
	ASSERT(src->getNumOutputs() >= 2);
	for (int i = 0; i < src->getNumOutputs(); ++i) {
		auto passthru = p.addModule<Passthru>();
		p.connect(GetOutputPin(src, i), passthru);
		auto sink = p.addModule<FakeSink>();
		p.connect(passthru, sink);
	}

	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: pipeline with split (join)") {
	Pipeline p;
	auto src = p.addNamedModule<Split>("split");
	auto sink = p.addNamedModule<FakeSink>("FakeSink");
	auto passthru1 = p.addNamedModule<Passthru>("Passthru1");
	auto passthru2 = p.addNamedModule<Passthru>("Passthru2");

	p.connect(GetOutputPin(src, 0), passthru1);
	p.connect(passthru1, sink, true);

	p.connect(GetOutputPin(src, 1), passthru2);
	p.connect(passthru2, sink, true);

	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: multiple inputs (send same packets to 2 inputs and check call count)") {
	Pipeline p;
	auto generator = p.addModule<FakeSource>(1);
	auto dualInput = p.addModule<ThreadedDualInput>();
	p.connect(generator, GetInputPin(dualInput, 0));
	p.connect(generator, GetInputPin(dualInput, 1));
	p.start();
	p.waitForEndOfStream();
	ASSERT_EQUALS(ThreadedDualInput::numCalls, 1u);
}
