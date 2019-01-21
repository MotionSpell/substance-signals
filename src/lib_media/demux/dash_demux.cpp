// holds the chain: [dash downloader] => ( [mp4demuxer] => [restamper] )*
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_modules/core/connection.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_modules/utils/factory.hpp"

std::unique_ptr<Modules::In::IFilePuller> createHttpSource();

using namespace Modules;
using namespace In;
using namespace Transform;

namespace {

class DashDemuxer : public Module {
	public:
		DashDemuxer(KHost* host, DashDemuxConfig* cfg)
			: m_host(host) {

			filePuller = createHttpSource();
			m_downloader = createModule<MPEG_DASH_Input>(m_host, filePuller.get(), cfg->url);

			for (int i = 0; i < m_downloader->getNumOutputs(); ++i)
				addStream(m_downloader->getOutput(i));
		}

		void process() override {
			m_downloader->process();
		}

	private:
		KHost* const m_host;
		std::unique_ptr<IFilePuller> filePuller;

		void addStream(IOutput* downloader) {
			// add MP4 demuxer
			std::shared_ptr<IModule> decap = loadModule("GPACDemuxMP4Full", m_host, nullptr);
			modules.push_back(decap);
			ConnectOutputToInput(downloader, decap->getInput(0));

			// add restamper (so the timestamps start at zero)
			std::shared_ptr<IModule> restamp = createModule<Restamp>(m_host, Transform::Restamp::Reset);
			modules.push_back(restamp);
			ConnectOutputToInput(decap->getOutput(0), restamp->getInput(0));

			// create our own output
			auto output = addOutput<OutputDefault>();
			output->setMetadata(downloader->getMetadata());

			auto deliver = [output](Data data) {
				output->post(data);
			};

			ConnectOutput(restamp->getOutput(0), deliver);
		}

		std::vector<std::shared_ptr<IModule>> modules;
		std::shared_ptr<IModule> m_downloader;
};

IModule* createObject(KHost* host, void* va) {
	auto config = (DashDemuxConfig*)va;
	enforce(host, "DashDemuxer: host can't be NULL");
	enforce(config, "DashDemuxer: config can't be NULL");
	return createModule<DashDemuxer>(host, config).release();
}

auto const registered = Factory::registerModule("DashDemuxer", &createObject);
}
