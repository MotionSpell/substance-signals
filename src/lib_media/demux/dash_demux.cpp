#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/demux/gpac_demux_mp4_full.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/restamp.hpp"

#include "lib_media/in/mpeg_dash_input.hpp" // IFilePuller

std::unique_ptr<Modules::In::IFilePuller> createHttpSource();

namespace Modules {
namespace Demux {

using namespace In;
using namespace Transform;

DashDemuxer::DashDemuxer(std::string url) {
	auto downloader = pipeline.addModule<MPEG_DASH_Input>(createHttpSource(), url);

	for (int i = 0; i < (int)downloader->getNumOutputs(); ++i)
		addStream(downloader->getOutput(i));
}

void DashDemuxer::addStream(IOutput* downloadOutput) {
	auto meta = downloadOutput->getMetadata();

	// create our own output
	auto output = addOutput<OutputDefault>();
	output->setMetadata(meta);

	// add MP4 demuxer
	auto decap = pipeline.addModule<GPACDemuxMP4Full>();
	ConnectOutputToInput(downloadOutput, decap->getInput(0));

	// add restamper (so the timestamps start at zero)
	auto restamp = pipeline.addModule<Restamp>(Transform::Restamp::Reset);
	ConnectOutputToInput(decap->getOutput(0), restamp->getInput(0));

	ConnectOutput(restamp, [output](Data data) {
		output->emit(data);
	});

	auto null = pipeline.addModule<Out::Null>();
	pipeline.connect(restamp, 0, null, 0);
}

}
}
