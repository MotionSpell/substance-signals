// holds the chain: [dash downloader] => ( [mp4demuxer] => [restamper] )*
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_pipeline/pipeline.hpp"

std::unique_ptr<Modules::In::IFilePuller> createHttpSource();

namespace Modules {
namespace Demux {

using namespace In;
using namespace Transform;

struct OutStub : ModuleS {
		OutStub(KHost*, OutputDefault *output) : output(output) {
			addInput(this);
		}
		void process(Data data) override {
			output->emit(data);
		}

	private:
		OutputDefault *output;
};

DashDemuxer::DashDemuxer(KHost* host, std::string url)
	: m_host(host) {
	(void)m_host;
	pipeline = make_unique<Pipelines::Pipeline>();
	auto downloader = pipeline->addModule<MPEG_DASH_Input>(createHttpSource(), url);

	for (int i = 0; i < (int)downloader->getNumOutputs(); ++i)
		addStream(downloader, i);
}

void DashDemuxer::addStream(Pipelines::IFilter* downloadOutput, int outputPort) {
	// create our own output
	auto output = addOutput<OutputDefault>();
	output->setMetadata(downloadOutput->getOutputMetadata(outputPort));

	// add MP4 demuxer
	auto decap = pipeline->add("GPACDemuxMP4Full", nullptr);
	pipeline->connect(GetOutputPin(downloadOutput, outputPort), decap);

	// add restamper (so the timestamps start at zero)
	auto restamp = pipeline->addModule<Restamp>(Transform::Restamp::Reset);
	pipeline->connect(decap, restamp);

	auto stub = pipeline->addModule<OutStub>(output);
	pipeline->connect(restamp, stub);
}

bool DashDemuxer::work() {
	pipeline->start();
	pipeline->waitForEndOfStream();
	return true;
}

}
}

