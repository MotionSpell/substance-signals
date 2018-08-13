#pragma once

#include <lib_modules/utils/helper.hpp>

namespace Pipelines {
class Pipeline;
struct IPipelinedModule;
}

namespace Modules {
namespace Demux {

class DashDemuxer : public ActiveModule {
	public:
		DashDemuxer(std::string url);

		virtual bool work() override;

	private:
		void addStream(Pipelines::IPipelinedModule* downloadOutput, int outputPort);

		std::unique_ptr<Pipelines::Pipeline> pipeline;
};

}
}
