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
	auto stub = p.addModule<Stub>();
	p.connect(src, 0, stub, 0);
	p.start();
	auto f = [&]() {
		p.exitSync();
	};
	std::thread tf(f);
	p.waitForEndOfStream();
	tf.join();
}

unittest("[DISABLED] pipeline: destroy while running") {
	Pipeline p;
	auto src = p.addModule<InfiniteSource>();
	auto stub = p.addModule<Stub>();
	p.connect(src, 0, stub, 0);
	p.start();
}

unittest("pipeline: intercept exception") {
	struct ExceptionModule : ModuleS {
		ExceptionModule() {
			addInput(new Input<DataBase>(this));
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
