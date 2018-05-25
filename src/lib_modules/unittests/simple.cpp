#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include <stdexcept>
#include <iostream>
#include "lib_media/demux/gpac_demux_mp4_simple.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/out/print.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

unittest("empty param test: File") {
	ASSERT_THROWN(create<In::File>(""));
}

unittest("empty param test: Demux") {
	ASSERT_THROWN(create<Demux::GPACDemuxMP4Simple>(""));
}

unittest("empty param test: Out::Print") {
	auto p = create<Out::Print>(std::cout);
}

unittest("simple param test") {
	auto f = create<In::File>("data/beepbop.mp4");
}

unittest("print packets size from file: File -> Out::Print") {
	auto f = create<In::File>("data/beepbop.mp4");
	auto p = create<Out::Print>(std::cout);

	ConnectOutputToInput(f->getOutput(0), p->getInput(0));

	f->process(nullptr);
}
