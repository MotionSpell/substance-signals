// holds the chain: [dash downloader] => ( [mp4demuxer] => [restamper] )*
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_pipeline/pipeline.hpp"

std::unique_ptr<Modules::In::IFilePuller> createHttpSource();

using namespace Modules;
using namespace In;
using namespace Transform;

namespace {

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

class DashDemuxer : public ActiveModule {
	public:
		DashDemuxer(KHost* host, DashDemuxConfig* cfg);
		~DashDemuxer();

		virtual bool work() override;

	private:
		void addStream(Pipelines::IFilter* downloadOutput, int outputPort);

		KHost* const m_host;
		std::unique_ptr<Pipelines::Pipeline> pipeline;
		std::unique_ptr<Modules::In::IFilePuller> filePuller;
};

DashDemuxer::DashDemuxer(KHost* host, DashDemuxConfig* cfg)
	: m_host(host) {
	(void)m_host;

	filePuller = createHttpSource();
	pipeline = make_unique<Pipelines::Pipeline>();
	auto downloader = pipeline->addModule<MPEG_DASH_Input>(filePuller.get(), cfg->url);

	for (int i = 0; i < (int)downloader->getNumOutputs(); ++i)
		addStream(downloader, i);
}

DashDemuxer::~DashDemuxer() {
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

Modules::IModule* createObject(KHost* host, void* va) {
	auto config = (DashDemuxConfig*)va;
	enforce(host, "DashDemuxer: host can't be NULL");
	enforce(config, "DashDemuxer: config can't be NULL");
	return Modules::create<DashDemuxer>(host, config).release();
}

auto const registered = Factory::registerModule("DashDemuxer", &createObject);
}
