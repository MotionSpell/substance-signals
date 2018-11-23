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

unittest("pipeline: destroy while running") {
	auto p = std::make_unique<Pipeline>();
	auto src = p->addModule<InfiniteSource>();
	auto sink = p->addModule<FakeSink>();
	p->connect(src, sink);
	p->start();
}

