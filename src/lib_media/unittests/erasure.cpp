#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/decode/decoder.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/out/print.hpp"
#include "lib_utils/tools.hpp"
#include <iostream> // std::cout

using namespace Tests;
using namespace Modules;

namespace {

secondclasstest("packet type erasure + multi-output: libav Demux -> {libav Decoder -> Out::Print}*") {
	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");

	std::vector<std::unique_ptr<Decode::Decoder>> decoders;
	std::vector<std::unique_ptr<Out::Print>> printers;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = demux->getOutput(i)->getMetadata();
		auto decode = create<Decode::Decoder>(metadata->getStreamType());

		auto p = create<Out::Print>(std::cout);

		ConnectOutputToInput(demux->getOutput(i), decode->getInput(0));
		ConnectOutputToInput(decode->getOutput(0), p->getInput(0));

		decoders.push_back(std::move(decode));
		printers.push_back(std::move(p));
	}

	demux->process(nullptr);
}

}
