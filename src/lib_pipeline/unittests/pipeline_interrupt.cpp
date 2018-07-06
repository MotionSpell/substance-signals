#include "tests/tests.hpp"
#include "lib_media/out/null.hpp"
#include "lib_pipeline/pipeline.hpp"

using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {
struct InfiniteSource : ActiveModule {
	InfiniteSource() {
		out = addOutput<OutputDefault>();
	}
	bool work() {
		out->emit(out->getBuffer(0));
		return true;
	}
	OutputDefault* out;
};
}

unittest("pipeline: EOS injection (exitSync)") {
	Pipeline p;
	auto demux = p.addModule<InfiniteSource>();
	auto null = p.addModule<Out::Null>();
	p.connect(demux, 0, null, 0);
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
	auto demux = p.addModule<InfiniteSource>();
	auto null = p.addModule<Out::Null>();
	p.connect(demux, 0, null, 0);
	p.start();
}

unittest("pipeline: intercept exception") {
	struct FakeSource : ActiveModule {
		FakeSource() {
			out = addOutput<OutputDefault>();
		}
		bool work() {
			out->emit(out->getBuffer(0));
			return ++i < 50;
		}
		int i = 0;
		OutputDefault* out;
	};
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
	auto demux = p.addModule<FakeSource>();
	p.connect(demux, 0, exception, 0);
	p.start();
	ASSERT_THROWN(p.waitForEndOfStream());
}

