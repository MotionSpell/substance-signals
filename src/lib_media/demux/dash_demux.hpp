#pragma once

#include <lib_modules/core/module.hpp>
#include <lib_modules/utils/helper.hpp>

namespace Pipelines {
class Pipeline;
struct IFilter;
}

namespace Modules {
namespace In {
struct IFilePuller;
}
}

namespace Modules {
namespace Demux {

class DashDemuxer : public ActiveModule {
	public:
		DashDemuxer(KHost* host, std::string url);
		~DashDemuxer();

		virtual bool work() override;

	private:
		void addStream(Pipelines::IFilter* downloadOutput, int outputPort);

		KHost* const m_host;
		std::unique_ptr<Pipelines::Pipeline> pipeline;
		std::unique_ptr<Modules::In::IFilePuller> filePuller;
};

}
}
