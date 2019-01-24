#include "tests/tests.hpp"
#include "pipeline_common.hpp"
#include "lib_pipeline/pipeline.hpp"
#include <thread>

using namespace Tests;
using namespace Modules;
using namespace Pipelines;

unittest("pipeline: EOS injection (exitSync)") {
	Pipeline p;
	auto src = p.addModule<InfiniteSource>();
	auto sink = p.addModule<FakeSink>();
	p.connect(src, sink);
	p.start();
	auto f = [&]() {
		p.exitSync();
	};
	std::thread tf(f);
	p.waitForEndOfStream();
	tf.join();
}

unittest("pipeline: destroy while running: easy") {
	auto p = std::make_unique<Pipeline>();
	auto src = p->addModule<InfiniteSource>();
	auto sink = p->addModule<FakeSink>();
	p->connect(src, sink);
	p->start();
}

struct FakeTransformer : public Modules::ModuleS {
	FakeTransformer(Modules::KHost*) {
		addInput();
		out = addOutput<Modules::OutputDefault>();
	}
	void process(Modules::Data) override {
		out->post(out->getBuffer(0));
	}
	Modules::OutputDefault* out;
};

unittest("[DISABLED] pipeline: destroy while running: long chain of modules") {
	for(int i=0; i< 10; ++i) {
		auto p = std::make_unique<Pipeline>();
		auto src = p->addModule<InfiniteSource>();
		auto trans1 = p->addModule<FakeTransformer>();
		auto trans2 = p->addModule<FakeTransformer>();
		auto trans3 = p->addModule<FakeTransformer>();
		auto sink = p->addModule<FakeSink>();
		p->connect(src, trans1);
		p->connect(trans1, trans2);
		p->connect(trans2, trans3);
		p->connect(trans3, sink);
		p->start();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

