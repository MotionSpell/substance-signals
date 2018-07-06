#pragma once

#include "pipeline.hpp"
#include "lib_modules/core/module.hpp"

using namespace Modules;

namespace Pipelines {

/* wrapper around the module */
class PipelinedModule :
	public IPipelinedModule,
	private ClockCap,
	private InputCap,
	private IPipelineNotifier {
	public:
		/* take ownership of module and executor */
		PipelinedModule(std::unique_ptr<IModule> module, IPipelineNotifier *notify, const std::shared_ptr<IClock> clock, Pipeline::Threading threading);
		~PipelinedModule() noexcept(false);

		int getNumInputs() const override;
		int getNumOutputs() const override;
		IOutput* getOutput(int i) override;

		/* source modules are stopped manually - then the message propagates to other connected modules */
		bool isSource() override;
		void stopSource() override;

	private:
		std::string getDelegateName() const;

		void connect(IOutput *output, int inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) override ;
		void disconnect(int inputIdx, IOutput * const output) override ;
		void mimicInputs() ;

		IInput* getInput(int i) override;

		/* uses the executor (i.e. may defer the call) */
		void process();

		// IPipelineNotifier implementation

		void endOfStream() override;
		void exception(std::exception_ptr eptr) override ;

		std::unique_ptr<IModule> delegate;
		std::unique_ptr<IProcessExecutor> const localDelegateExecutor;
		IProcessExecutor &delegateExecutor;

		std::vector<IProcessExecutor*> inputExecutor; /*needed to sleep when using a clock*/
		Pipeline::Threading threading;

		IPipelineNotifier * const m_notify;
		int connections = 0;
		std::atomic<int> eosCount;
};

}
