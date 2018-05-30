#pragma once

#include "pipeline.hpp"
#include "lib_modules/core/module.hpp"

using namespace Modules;

namespace Pipelines {

/* automatic inputs have a loose datatype */
struct DataLoosePipeline : public DataBase {};

/* wrapper around the module */
class PipelinedModule : public IPipelineNotifier, public IPipelinedModule, private ClockCap, private InputCap {
	public:
		/* take ownership of module and executor */
		PipelinedModule(std::unique_ptr<IModule> module, IPipelineNotifier *notify, const std::shared_ptr<IClock> clock, Pipeline::Threading threading);
		~PipelinedModule() noexcept(false) {}

		size_t getNumInputs() const override;
		size_t getNumOutputs() const override;
		IOutput* getOutput(size_t i) override;

		/* source modules are stopped manually - then the message propagates to other connected modules */
		bool isSource() override;
		bool isSink() override;

	private:
		std::string getDelegateName() const;

		void connect(IOutput *output, size_t inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) override ;
		void disconnect(size_t inputIdx, IOutput * const output) override ;
		void mimicInputs() ;

		IInput* getInput(size_t i) override;

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
