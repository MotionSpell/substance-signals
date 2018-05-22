#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/demux/gpac_demux_mp4_simple.hpp"
#include "lib_media/demux/gpac_demux_mp4_full.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/out/print.hpp"
#include "lib_utils/tools.hpp"
#include <iostream> // std::cout

using namespace Tests;
using namespace Modules;

namespace {

secondclasstest("demux one track: Demux::GPACDemuxMP4Simple -> Out::Print") {
	auto mp4Demux = create<Demux::GPACDemuxMP4Simple>("data/beepbop.mp4");
	auto p = create<Out::Print>(std::cout);

	ConnectOutputToInput(mp4Demux->getOutput(0), p->getInput(0));

	mp4Demux->process(nullptr);
}

secondclasstest("[DISABLED] demux one track: File -> Demux::GPACDemuxMP4Full -> Out::Print") {
	auto f = create<In::File>("data/beepbop.mp4");
	auto mp4Demux = create<Demux::GPACDemuxMP4Full>();
	auto p = create<Out::Print>(std::cout);

	ConnectOutputToInput(f->getOutput(0), mp4Demux->getInput(0));
	ConnectOutputToInput(mp4Demux->getOutput(0), p->getInput(0));

	f->process(nullptr);
}

}
