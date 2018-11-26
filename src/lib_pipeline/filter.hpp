#pragma once

#include "pipeline.hpp"
#include "lib_modules/core/module.hpp"

using namespace Modules;

namespace Pipelines {

/* wrapper around the module */
class Filter :
	public IFilter,
	private IPipelineNotifier {
	public:
		Filter(const char* name,
		    std::unique_ptr<KHost> host,
		    std::shared_ptr<IModule> module,
		    IPipelineNotifier *notify,
		    Pipeline::Threading threading,
		    IStatsRegistry *statsRegistry);

		void connect(IOutput *output, int inputIdx, bool inputAcceptMultipleConnections);
		void disconnect(int inputIdx, IOutput* output);

		int getNumInputs() const override;
		int getNumOutputs() const override;
		IInput* getInput(int i);
		IOutput* getOutput(int i);
		Metadata getOutputMetadata(int i) override;

		/* source modules are stopped manually - then the message propagates to other connected modules */
		bool isSource();

		void startSource();
		void stopSource();

	private:
		void mimicInputs();
		void processSource();

		// IPipelineNotifier implementation
		void endOfStream() override;
		void exception(std::exception_ptr eptr) override;

		std::unique_ptr<KHost> m_host;
		std::string const m_name;
		std::shared_ptr<IModule> delegate;
		std::unique_ptr<IExecutor> const executor;

		bool started = false;
		bool stopped = false;

		IPipelineNotifier * const m_notify;
		int connections = 0;
		std::atomic<int> eosCount;

		IStatsRegistry * const statsRegistry;

		std::vector<std::unique_ptr<IInput>> inputs;
};

}
