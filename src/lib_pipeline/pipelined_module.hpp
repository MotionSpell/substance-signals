#pragma once

#include "pipeline.hpp"
#include "pipelined_input.hpp"
#include "lib_modules/modules.hpp"
#include <typeinfo>

using namespace Modules;

namespace Pipelines {

/* automatic inputs have a loose datatype */
struct DataLoosePipeline : public DataBase {};

/* Wrapper around the module. */
class PipelinedModule : public IPipelineNotifier, public IPipelinedModule, public InputCap {
public:
	/* take ownership of module and executor */
	PipelinedModule(IModule *module, IPipelineNotifier *notify, IClock const * const clock, Pipeline::Threading threading)
		: delegate(module), localDelegateExecutor(threading & Pipeline::Mono ? (IProcessExecutor*)new EXECUTOR_LIVE : (IProcessExecutor*)new EXECUTOR),
		delegateExecutor(*localDelegateExecutor), clock(clock), threading(threading), m_notify(notify), activeConnections(0) {
	}
	~PipelinedModule() noexcept(false) {}
	std::string getDelegateName() const {
		auto const &dref = *delegate.get();
		return typeid(dref).name();
	}

	size_t getNumInputs() const override {
		return delegate->getNumInputs();
	}
	size_t getNumOutputs() const override {
		return delegate->getNumOutputs();
	}
	IOutput* getOutput(size_t i) const override {
		if (i >= delegate->getNumOutputs())
			throw std::runtime_error(format("PipelinedModule %s: no output %s.", getDelegateName(), i));
		return delegate->getOutput(i);
	}

	/* source modules are stopped manually - then the message propagates to other connected modules */
	bool isSource() const override {
		if (delegate->getNumInputs() == 0) {
			return true;
		} else if (delegate->getNumInputs() == 1 && dynamic_cast<Input<DataLoosePipeline, IProcessor>*>(delegate->getInput(0))) {
			return true;
		} else {
			return false;
		}
	}
	bool isSink() const override {
		for (size_t i = 0; i < getNumOutputs(); ++i) {
			if (getOutput(i)->getSignal().getNumConnections() > 0)
				return false;
		}
		return true;
	}

private:
	void connect(IOutput *output, size_t inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) override {
		auto input = safe_cast<PipelinedInput>(getInput(inputIdx));
		if (forceAsync && !(threading & Pipeline::RegulationOffFlag) && (inputExecutor[inputIdx] == EXECUTOR_INPUT_DEFAULT)) {
			auto executor = uptr(new REGULATION_EXECUTOR);
			inputExecutor[inputIdx] = executor.get();
			input->setLocalExecutor(std::move(executor));
		}
		ConnectOutputToInput(output, input, inputExecutor[inputIdx]);
		if (!inputAcceptMultipleConnections && (input->getNumConnections() != 1))
			throw std::runtime_error(format("PipelinedModule %s: input %s has %s connections.", getDelegateName(), inputIdx, input->getNumConnections()));
		activeConnections++;
	}

	void mimicInputs() {
		auto const delegateInputs = delegate->getNumInputs();
		auto const thisInputs = inputs.size();
		if (thisInputs < delegateInputs) {
			for (size_t i = thisInputs; i < delegateInputs; ++i) {
				inputExecutor.push_back(EXECUTOR_INPUT_DEFAULT);
				addInput(new PipelinedInput(delegate->getInput(i), getDelegateName(), inputExecutor[i], this->delegateExecutor, this, clock));
			}
		}
	}

	IInput* getInput(size_t i) override {
		mimicInputs();
		if (i >= inputs.size())
			throw std::runtime_error(format("PipelinedModule %s: no input %s.", getDelegateName(), i));
		return inputs[i].get();
	}

	/* uses the executor (i.e. may defer the call) */
	void process() override {
		Log::msg(Debug, "Module %s: dispatch data", getDelegateName());

		if (isSource()) {
			if (getNumInputs() == 0) {
				/*first time: create a fake pin and push null to trigger execution*/
				delegate->addInput(new Input<DataLoosePipeline>(delegate.get()));
				getInput(0)->push(nullptr);
				delegate->getInput(0)->push(nullptr);
				delegateExecutor(MEMBER_FUNCTOR_PROCESS(delegate.get()));
				delegateExecutor(MEMBER_FUNCTOR_PROCESS(getInput(0)));
				return;
			} else {
				/*the source is likely processing: push null in the loop to exit and let things follow their way*/
				delegate->getInput(0)->push(nullptr);
				return;
			}
		}

		Data data = getInput(0)->pop();
		for (size_t i = 0; i < getNumInputs(); ++i) {
			getInput(i)->push(data);
			getInput(i)->process();
		}
	}

	void finished() override {
		if (--activeConnections <= 0) {
			delegate->flush();
			if (isSink()) {
				m_notify->finished();
			} else {
				for (size_t i = 0; i < delegate->getNumOutputs(); ++i) {
					delegate->getOutput(i)->emit(nullptr);
				}
			}
		}
	}

	void exception(std::exception_ptr eptr) override {
		m_notify->exception(eptr);
	}

	std::unique_ptr<IModule> delegate;
	std::unique_ptr<IProcessExecutor> const localDelegateExecutor;
	IProcessExecutor &delegateExecutor;

	std::vector<IProcessExecutor*> inputExecutor; /*needed to sleep when using a clock*/
	IClock const * const clock;
	Pipeline::Threading threading;

	IPipelineNotifier * const m_notify;
	std::atomic<int> activeConnections;
};

}
