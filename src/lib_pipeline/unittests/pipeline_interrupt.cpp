#include "tests/tests.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/out/null.hpp"
#include "lib_pipeline/pipeline.hpp"

using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {

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

class ExceptionModule : public ModuleS {
	public:
		ExceptionModule() {
			addInput(new Input<DataBase>(this));
		}
		void process(Data) {
			if (!raised) {
				raised = true;
				throw error("Test exception");
			}
		}

	private:
		bool raised = false;
};

unittest("[DISABLED] pipeline: intercept exception") {
	bool thrown = false;
	try {
		Pipeline p;
		auto exception = p.addModule<ExceptionModule>();
		auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
		p.connect(demux, 0, exception, 0);
		p.start();
		p.waitForCompletion();
	} catch (...) {
		thrown = true;
	}
	ASSERT(thrown);
}

}
