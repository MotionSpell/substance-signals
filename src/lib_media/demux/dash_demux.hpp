#pragma once

#include <lib_modules/utils/helper.hpp>

namespace Pipelines {
	class Pipeline;
	struct IPipelinedModule;
}

namespace Modules {
namespace Demux {

class DashDemuxer : public Module {
	public:
		DashDemuxer(std::string url);

		virtual void process() override;

	private:
		void addStream(IOutput* downloadOutput);

		std::unique_ptr<Pipelines::Pipeline> pipeline;
};

}
}
