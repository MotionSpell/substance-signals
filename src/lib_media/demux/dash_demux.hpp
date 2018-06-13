#pragma once

#include "lib_pipeline/pipeline.hpp"

namespace Modules {
namespace Demux {

class DashDemuxer : public Module {
	public:
		DashDemuxer(std::string url);

		virtual void process() override {
			pipeline.start();
			pipeline.waitForEndOfStream();
		}

	private:
		void addStream(IOutput* downloadOutput);

		Pipelines::Pipeline pipeline;
};

}
}
