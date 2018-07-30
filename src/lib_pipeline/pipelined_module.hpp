#pragma once

#include "pipeline.hpp"
#include "lib_modules/core/module.hpp"

using namespace Modules;

namespace Pipelines {

/* wrapper around the module */
class PipelinedModule :
	public IPipelinedModule,
	private InputCap,
	private IPipelineNotifier {
	public:
		/* take ownership of module and executor */
		PipelinedModule(const char* name, std::unique_ptr<IModuleHost> host, std::shared_ptr<IModule> module, IPipelineNotifier *notify, Pipeline::Threading threading, IStatsRegistry *statsRegistry);
		~PipelinedModule();

		void connect(IOutput *output, int inputIdx, bool inputAcceptMultipleConnections);
		void disconnect(int inputIdx, IOutput * const output);

		int getNumInputs() const override;
		int getNumOutputs() const override;
		IInput* getInput(int i) override;
		IOutput* getOutput(int i);
		std::shared_ptr<const IMetadata>  getOutputMetadata(int i) override;

		/* source modules are stopped manually - then the message propagates to other connected modules */
		bool isSource();
		/* uses the executor (i.e. may defer the call) */
		void startSource();
		void stopSource();

	private:
		void mimicInputs();


		// IPipelineNotifier implementation
		void endOfStream() override;
		void exception(std::exception_ptr eptr) override;

		std::unique_ptr<IModuleHost> m_host;
		std::string const m_name;
		std::shared_ptr<IModule> delegate;
		std::unique_ptr<IExecutor> const executor;

		std::vector<IExecutor*> inputExecutor; /*needed to sleep when using a clock*/
		bool started = false;

		IPipelineNotifier * const m_notify;
		int connections = 0;
		std::atomic<int> eosCount;

		IStatsRegistry * const statsRegistry;
};

}
