#pragma once

#include "pipeline.hpp"
#include "pipelined_input.hpp"
#include "lib_modules/modules.hpp"
#include <typeinfo>

using namespace Modules;

namespace Pipelines {

/* automatic inputs have a loose datatype */
struct DataLoosePipeline : public DataBase {};

/* wrapper around the module */
class PipelinedModule : public IPipelineNotifier, public ClockCap, public IPipelinedModule, private Module {
	public:
		/* take ownership of module and executor */
		PipelinedModule(std::unique_ptr<IModule> module, IPipelineNotifier *notify, const std::shared_ptr<IClock> clock, Pipeline::Threading threading)
			: ClockCap(clock),
			  delegate(std::move(module)), localDelegateExecutor(threading & Pipeline::Mono ? (IProcessExecutor*)new EXECUTOR_LIVE : (IProcessExecutor*)new EXECUTOR),
			  delegateExecutor(*localDelegateExecutor), threading(threading), m_notify(notify), activeConnections(0) {
		}
		~PipelinedModule() noexcept(false) {}
		void flush() override {
			process();
			while (activeConnections > 0) clock->sleep(Fraction(10, 1000));
		}
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
		IOutput* getOutput(size_t i) override {
			if (i >= delegate->getNumOutputs())
				throw std::runtime_error(format("PipelinedModule %s: no output %s.", getDelegateName(), i));
			return delegate->getOutput(i);
		}

		/* source modules are stopped manually - then the message propagates to other connected modules */
		bool isSource() override {
			if (delegate->getNumInputs() == 0) {
				return true;
			} else if (delegate->getNumInputs() == 1 && dynamic_cast<Input<DataLoosePipeline, IProcessor>*>(delegate->getInput(0))) {
				return true;
			} else {
				return false;
			}
		}
		bool isSink() override {
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
			connections++; activeConnections++;
		}

		void disconnect(size_t inputIdx, IOutput * const output) override {
			getInput(inputIdx)->disconnect();
			auto &sig = output->getSignal();
			auto const numConn = sig.getNumConnections();
			for (size_t i = 0; i < numConn; ++i) {
				sig.disconnect(i);
			}
			connections--; activeConnections--;
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
				if (getNumInputs() == 0) { /*first time: create a fake input port and push null to trigger execution*/
					safe_cast<InputCap>(delegate.get())->addInput(new Input<DataLoosePipeline>(delegate.get()));
					connections = 1; activeConnections = 1;
					getInput(0)->push(nullptr);
					delegate->getInput(0)->push(nullptr);
					delegateExecutor(Bind(&IProcessor::process, delegate.get()));
					delegateExecutor(Bind(&IProcessor::process, getInput(0)));
				} else { /*the source is likely processing: push null in the loop to exit and let things follow their way*/
					delegate->getInput(0)->push(nullptr);
				}
				return;
			}

			Data data = getInput(0)->pop();
			for (size_t i = 0; i < getNumInputs(); ++i) {
				getInput(i)->push(data);
				getInput(i)->process();
			}
		}

		void endOfStream() override {
			if (!connections || --activeConnections == 0) {
				delegate->flush();

				for (size_t i = 0; i < delegate->getNumOutputs(); ++i) {
					delegate->getOutput(i)->emit(nullptr);
				}

				if (isSink()) {
					if (connections) {
						m_notify->endOfStream();
					}
				}
			} else if (activeConnections < 0)
				throw std::runtime_error(format("PipelinedModule %s: activeConnections is negative (%s).", getDelegateName(), (int)activeConnections));
		}

		void exception(std::exception_ptr eptr) override {
			m_notify->exception(eptr);
		}

		std::unique_ptr<IModule> delegate;
		std::unique_ptr<IProcessExecutor> const localDelegateExecutor;
		IProcessExecutor &delegateExecutor;

		std::vector<IProcessExecutor*> inputExecutor; /*needed to sleep when using a clock*/
		Pipeline::Threading threading;

		IPipelineNotifier * const m_notify;
		size_t connections = 0;
		std::atomic<int> activeConnections;
};

}
