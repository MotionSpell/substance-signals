#include "pipeline.hpp"
#include "stranded_pool_executor.hpp"
#include <typeinfo>
#include "helper.hpp"

#define EXECUTOR_SYNC              Signals::ExecutorSync<void()>
#define EXECUTOR_ASYNC_THREAD      Signals::ExecutorThread<void()>(getDelegateName())
#define EXECUTOR_ASYNC_POOL        StrandedPoolModuleExecutor

#define EXECUTOR                   EXECUTOR_ASYNC_THREAD
#define EXECUTOR_LIVE              EXECUTOR_SYNC
#define EXECUTOR_INPUT_DEFAULT     (&g_executorSync)

#define REGULATION_EXECUTOR        EXECUTOR_ASYNC_THREAD
#define REGULATION_TOLERANCE_IN_MS 300

using namespace Modules;

namespace Pipelines {

namespace {
template<typename Class>
Signals::MemberFunctor<void, Class, void(Class::*)()>
MEMBER_FUNCTOR_NOTIFY_FINISHED(Class* objectPtr) {
	return Signals::MemberFunctor<void, Class, void(Class::*)()>(objectPtr, &ICompletionNotifier::finished);
}
}

/* Wrapper around the module's inputs.
   Data is queued in the calling thread, then always dispatched by the executor
       (with a delay if the clock is set: this assumes the connection is made
       using an asynchronous executor and the start time is zero).
   Data is nullptr at startup and at completion. */
class PipelinedInput : public IInput {
	public:
		PipelinedInput(IInput *input, const std::string &moduleName, IProcessExecutor *localExecutor, IProcessExecutor &delegateExecutor, IPipelineNotifier * const notify, IClock const * const clock)
			: delegate(input), delegateName(moduleName), notify(notify), executor(localExecutor), delegateExecutor(delegateExecutor), clock(clock) {}
		virtual ~PipelinedInput() noexcept(false) {}

		/* receiving nullptr stops the execution */
		void process() override {
			auto data = pop();
			if (data) {
				auto const dataTime = data->getTime();
				if (!dynamic_cast<EXECUTOR_SYNC*>(executor)) {
					regulate(dataTime);
				}
				Log::msg(Debug, "Module %s: dispatch data for time %ss", delegateName, dataTime / (double)IClock::Rate);
				delegate->push(data);
				try {
					delegateExecutor(MEMBER_FUNCTOR_PROCESS(delegate));
				} catch (...) { //stop now
					auto const &eptr = std::current_exception();
					notify->exception(eptr);
					std::rethrow_exception(eptr);
				}
			} else {
				Log::msg(Debug, "Module %s: notify finished.", delegateName);
				delegateExecutor(MEMBER_FUNCTOR_NOTIFY_FINISHED(notify));
			}
		}

		size_t getNumConnections() const override {
			return delegate->getNumConnections();
		}
		void connect() override {
			delegate->connect();
		}

		void setLocalExecutor(std::unique_ptr<IProcessExecutor> e) {
			localExecutor = std::move(e);
			executor = localExecutor.get();
		}

	private:
		void regulate(uint64_t dataTime) {
			if (clock->getSpeed() > 0.0) {
				auto const delayInMs = clockToTimescale((int64_t)(dataTime - clock->now()), 1000);
				if (delayInMs > 0) {
					Log::msg(delayInMs < REGULATION_TOLERANCE_IN_MS ? Debug : Warning, "Module %s: received data for time %ss (will sleep %s ms)", delegateName, dataTime / (double)IClock::Rate, delayInMs);
					std::this_thread::sleep_for(std::chrono::milliseconds(delayInMs));
				} else if (delayInMs + REGULATION_TOLERANCE_IN_MS < 0) {
					Log::msg(Warning, "Module %s: received data for time %ss is late from %sms", delegateName, dataTime / (double)IClock::Rate, -delayInMs);
				}
			}
		}

		IInput *delegate;
		std::string delegateName;
		IPipelineNotifier * const notify;
		IProcessExecutor *executor, &delegateExecutor;
		std::unique_ptr<IProcessExecutor> localExecutor;
		IClock const * const clock;
};

/* Wrapper around the module. */
class PipelinedModule : public IPipelineNotifier, public IPipelinedModule, public InputCap {
public:
	/* take ownership of module and executor */
	PipelinedModule(IModule *module, IPipelineNotifier *notify, IClock const * const clock, Pipeline::Threading threading)
		: delegate(module), localDelegateExecutor(threading & Pipeline::Mono ? (IProcessExecutor*)new EXECUTOR_LIVE : (IProcessExecutor*)new EXECUTOR),
		delegateExecutor(*localDelegateExecutor), clock(clock), threading(threading), m_notify(notify) {
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
		} else if (delegate->getNumInputs() == 1 && dynamic_cast<Input<DataLoose, IProcessor>*>(delegate->getInput(0))) {
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
				delegate->addInput(new Input<DataLoose>(delegate.get()));
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
		delegate->flush();
		if (isSink()) {
			m_notify->finished();
		} else {
			for (size_t i = 0; i < delegate->getNumOutputs(); ++i) {
				delegate->getOutput(i)->emit(nullptr);
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
};

Pipeline::Pipeline(bool isLowLatency, double clockSpeed, Threading threading)
: allocatorNumBlocks(isLowLatency ? Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY : Modules::ALLOC_NUM_BLOCKS_DEFAULT),
  clock(new Modules::Clock(clockSpeed)), threading(threading), numRemainingNotifications(0) {
}

IPipelinedModule* Pipeline::addModuleInternal(IModule *rawModule) {
	auto module = uptr(new PipelinedModule(rawModule, this, clock.get(), threading));
	auto ret = module.get();
	modules.push_back(std::move(module));
	return ret;
}

void Pipeline::connect(IModule *p, size_t outputIdx, IModule *n, size_t inputIdx, bool inputAcceptMultipleConnections) {
	if (!n || !p) return;
	auto next = safe_cast<IPipelinedModule>(n);
	auto prev = safe_cast<IPipelinedModule>(p);
	next->connect(prev->getOutput(outputIdx), inputIdx, prev->isSource(), inputAcceptMultipleConnections);
}

void Pipeline::start() {
	Log::msg(Info, "Pipeline: starting");
	computeTopology();
	for (auto &m : modules) {
		if (m->isSource()) {
			m->process();
		}
	}
	Log::msg(Info, "Pipeline: started");
}

void Pipeline::waitForCompletion() {
	Log::msg(Info, "Pipeline: waiting for completion");
	std::unique_lock<std::mutex> lock(mutex);
	while (numRemainingNotifications > 0) {
		Log::msg(Debug, "Pipeline: condition (remaining: %s) (%s modules in the pipeline)", (int)numRemainingNotifications, modules.size());
		condition.wait(lock);
		if (eptr) {
			//TODO: Pipeline::pause(); + Resume?()
			//std::rethrow_exception(eptr);
		}
	}
	Log::msg(Info, "Pipeline: completed");
}

void Pipeline::exitSync() {
	Log::msg(Warning, "Pipeline: asked to exit now.");
	for (auto &m : modules) {
		if (m->isSource()) {
			m->process();
		}
	}
}

void Pipeline::computeTopology() {
	for (auto &m : modules) {
		if (m->isSink()) {
			if (m->isSource()) {
				numRemainingNotifications++;
			} else {
				for (size_t i = 0; i < m->getNumInputs(); ++i) {
					numRemainingNotifications += m->getInput(i)->getNumConnections();
				}
			}
		}
	}
}

void Pipeline::finished() {
	std::unique_lock<std::mutex> lock(mutex);
	assert(numRemainingNotifications > 0);
	--numRemainingNotifications;
	condition.notify_one();
}

void Pipeline::exception(std::exception_ptr e) {
	std::unique_lock<std::mutex> lock(mutex);
	eptr = e;
	condition.notify_one();
}

}
