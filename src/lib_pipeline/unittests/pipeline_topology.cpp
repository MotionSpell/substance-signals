#include "tests/tests.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/video_generator.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "pipeline_common.hpp"

using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {

unittest("pipeline: source only") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	p.start();
	p.waitForEndOfStream();
}

unittest("[DISABLED] pipeline: a non left-connected module is not a source") {
	Pipeline p;
	auto data = make_shared<DataRaw>(0);
	auto null = p.addModule<Out::Null>();
	null->getInput(0)->push(data);
	null->getInput(0)->process();
	null->getInput(0)->push(nullptr);
	//FIXME: null->flush();
	//FIXME: ASSERT(!null->isSource());
	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: connect one input (out of 2) to one output") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto null = p.addModule<Out::Null>();
	p.connect(demux, 0, null, 0);
	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: connect two outputs to the same input") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto null = p.addModule<Out::Null>();
	p.connect(demux, 0, null, 0);
	p.connect(demux, 1, null, 0, true);
	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: connect passthru to a multiple input module (1)") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto passthru = p.addModule<Transform::Restamp>(Transform::Restamp::Passthru);
	auto dualInput = p.addModule<DualInput>();
	p.connect(demux, 0, passthru, 0);
	p.connect(passthru, 0, dualInput, 0);
	p.connect(passthru, 0, dualInput, 1);
	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: connect passthru to a multiple input module (2)") {
	Pipeline p;
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	ASSERT(demux->getNumOutputs() > 1);
	auto passthru0 = p.addModule<Transform::Restamp>(Transform::Restamp::Passthru);
	auto passthru1 = p.addModule<Transform::Restamp>(Transform::Restamp::Passthru);
	auto passthru2 = p.addModule<Transform::Restamp>(Transform::Restamp::Passthru);
	auto dualInput = p.addModule<DualInput>();
	p.connect(demux, 0, passthru0, 0);
	p.connect(passthru0, 0, passthru1, 0);
	p.connect(passthru0, 0, passthru2, 0);
	p.connect(passthru1, 0, dualInput, 0);
	p.connect(passthru2, 0, dualInput, 1);
	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: orphan dynamic inputs sink") {
	Pipeline p;
	auto src = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	auto sink = p.addModule<Out::Null>();
	p.connect(src, 0, sink, 0);
	p.addModule<Mux::LibavMux>("orphan", "mp4");
	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: sink only (incorrect topology)") {
	Pipeline p;
	p.addModule<Out::Null>();
	p.start();
	p.waitForEndOfStream();
}

unittest("pipeline: null after split") {
	Pipeline p;
	auto generator = p.addModule<In::VideoGenerator>(10);
	auto dualInput = p.addModule<ThreadedDualInput>();
	p.connect(generator, 0, dualInput, 0);
	p.connect(generator, 0, dualInput, 1);
	auto passthru = p.addModule<Transform::Restamp>(Transform::Restamp::Passthru);
	p.connect(dualInput, 0, passthru, 0);
	p.start();
	p.waitForEndOfStream();
}

}
