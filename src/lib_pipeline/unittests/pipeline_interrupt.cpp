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
	p.connect(src, 0, sink, 0);
	p.start();
	auto f = [&]() {
		p.exitSync();
	};
	std::thread tf(f);
	p.waitForEndOfStream();
	tf.join();
}

unittest("pipeline: destroy while running") {
	auto p = std::make_unique<Pipeline>();
	auto src = p->addModule<InfiniteSource>();
	auto sink = p->addModule<FakeSink>();
	p->connect(src, 0, sink, 0);
	p->start();
}

unittest("pipeline: intercept exception") {
	struct ExceptionModule : ModuleS {
		ExceptionModule() {
			createInput(this);
		}
		void process(Data) {
			if (!raised) {
				raised = true;
				throw error("Test exception");
			}
		}
		bool raised = false;
	};

	ScopedLogLevel lev(Quiet);
	Pipeline p;
	auto exception = p.addModule<ExceptionModule>();
	auto src = p.addModule<FakeSource>();
	p.connect(src, 0, exception, 0);
	p.start();
	ASSERT_THROWN(p.waitForEndOfStream());
}
