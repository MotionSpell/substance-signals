#include "pipeline.hpp"
#include "stranded_pool_executor.hpp"
#include <typeinfo>
#include "helper.hpp"

#define EXECUTOR_SYNC         Signals::ExecutorSync<void()>
#define EXECUTOR_ASYNC_THREAD Signals::ExecutorThread<void()>(getDelegateName())
#define EXECUTOR_ASYNC_POOL   StrandedPoolModuleExecutor
#define EXECUTOR EXECUTOR_ASYNC_THREAD

#define REGULATION_EXECUTOR EXECUTOR_ASYNC_THREAD
#define REGULATION_TOLERANCE_IN_MS 200
#define PROBE_TIMEOUT_IN_MS 20

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
       using an asynchronous executor).
   Data is nullptr at startup (probing topology) and at completion. */
class PipelinedInput : public IInput {
	public:
		PipelinedInput(IInput *input, IProcessExecutor &executor, IPipelineNotifier * const notify, IClock const * const clock)
			: delegate(input), notify(notify), executor(executor), clock(clock) {}
		virtual ~PipelinedInput() noexcept(false) {}

		/* receiving nullptr stops the execution */
		void process() override {
			auto data = pop();
			if (data) {
				auto const dataTime = data->getTime();
				if (probeState) {
					assert(dataTime == 0);
					probeState = false;
				}
				regulate(dataTime);
				Log::msg(Debug, "Module %s: dispatch data for time %s", typeid(delegate).name(), dataTime / (double)IClock::Rate);
				delegate->push(data);
				executor(MEMBER_FUNCTOR_PROCESS(delegate));
			} else if (probeState && getNumConnections()) {
				Log::msg(Debug, "Module %s: probing.", typeid(delegate).name());
				notify->probe();
			} else {
				Log::msg(Debug, "Module %s: notify finished.", typeid(delegate).name());
				executor(MEMBER_FUNCTOR_NOTIFY_FINISHED(notify));
			}
		}

		size_t getNumConnections() const override {
			return delegate->getNumConnections();
		}
		void connect() override {
			delegate->connect();
		}

	private:
		void regulate(uint64_t dataTime) {
			if (clock->getSpeed() > 0.0) {
				auto const delayInMs = clockToTimescale((int64_t)(dataTime - clock->now()), 1000);
				if (delayInMs > 0) {
					Log::msg(delayInMs < REGULATION_TOLERANCE_IN_MS ? Debug : Warning, "Module %s: received data for time %s (will sleep %s ms)", typeid(delegate).name(), dataTime / (double)IClock::Rate, delayInMs);
					std::this_thread::sleep_for(std::chrono::milliseconds(delayInMs));
				} else if (delayInMs + REGULATION_TOLERANCE_IN_MS < 0) {
					Log::msg(Warning, "Module %s: received data for time %s is late from %s", typeid(delegate).name(), dataTime / (double)IClock::Rate, delayInMs);
				}
			}
		}

		IInput *delegate;
		IPipelineNotifier * const notify;
		bool probeState = true;
		IProcessExecutor &executor;
		IClock const * const clock;
};

/* Wrapper around the module. */
class PipelinedModule : public IPipelineNotifier, public IPipelinedModule, public InputCap {
public:
	/* take ownership of module */
	PipelinedModule(IModule *module, IPipelineNotifier *notify, IClock const * const clock)
		: delegate(module), localDelegateExecutor(new EXECUTOR), delegateExecutor(*localDelegateExecutor), clock(clock), m_notify(notify) {
	}
	~PipelinedModule() noexcept(false) {}
	std::string getDelegateName() const {
		return typeid(*delegate).name();
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
	void connect(IOutput *output, size_t inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) {
		auto input = getInput(inputIdx);
		if (forceAsync && inputExecutor[inputIdx] == &g_executorSync) {
			localInputExecutor[inputIdx] = uptr(new REGULATION_EXECUTOR);
			inputExecutor[inputIdx] = localInputExecutor[inputIdx].get();
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
				addInput(new PipelinedInput(delegate->getInput(i), this->delegateExecutor, this, clock));
				inputExecutor.push_back(&g_executorSync);
			}
			localInputExecutor.resize(delegateInputs);
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

	void propagate() {
		for (size_t i = 0; i < delegate->getNumOutputs(); ++i) {
			delegate->getOutput(i)->emit(nullptr);
		}
	}

	void probe() override {
		if (isSink()) {
			m_notify->probe();
		} else {
			propagate();
		}
	}

	void finished() override {
		delegate->flush();
		if (isSink()) {
			m_notify->finished();
		} else {
			propagate();
		}
	}

	std::unique_ptr<IModule> delegate;
	std::unique_ptr<IProcessExecutor> const localDelegateExecutor;
	IProcessExecutor &delegateExecutor;

	std::vector<IProcessExecutor*> inputExecutor; /*needed to sleep when using a clock*/
	std::vector<std::unique_ptr<IProcessExecutor>> localInputExecutor;
	IClock const * const clock;

	IPipelineNotifier * const m_notify;
};

Pipeline::Pipeline(bool isLowLatency, double clockSpeed)
: isLowLatency(isLowLatency), clock(new Modules::Clock(clockSpeed)), numRemainingNotifications(0) {
}

IPipelinedModule* Pipeline::addModuleInternal(IModule *rawModule) {
	auto module = uptr(new PipelinedModule(rawModule, this, clock.get()));
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

void Pipeline::startSources() {
	Log::msg(Debug, "Pipeline: start sources");
	for (auto &m : modules) {
		if (m->isSource()) {
			m->process();
		}
	}
	Log::msg(Debug, "Pipeline: sources started");
}

void Pipeline::computeNotifications() {
	Log::msg(Debug, "Pipeline: check for sinks by propagation");
	for (auto &m : modules) {
		if (m->isSource()) {
			if (m->isSink()) {
				numRemainingNotifications++;
			} else {
				for (size_t i = 0; i < m->getNumOutputs(); ++i) {
					m->getOutput(i)->emit(nullptr);
				}
			}
		}
	}
	{
		std::unique_lock<std::mutex> lock(mutex);
		while (condition.wait_for(lock, std::chrono::milliseconds(PROBE_TIMEOUT_IN_MS)) != std::cv_status::timeout) {}
	}
	if (modules.size() && !numRemainingNotifications)
		throw std::runtime_error(format("Pipelined: no notification found. Check the topology of your graph."));
	Log::msg(Debug, format("Pipeline: %s sinks notifications detected", numRemainingNotifications));
}

void Pipeline::start() {
	Log::msg(Info, "Pipeline: starting");
	computeNotifications();
	startSources();
	Log::msg(Info, "Pipeline: started");
}

void Pipeline::waitForCompletion() {
	Log::msg(Info, "Pipeline: waiting for completion");
	std::unique_lock<std::mutex> lock(mutex);
	while (numRemainingNotifications > 0) {
		Log::msg(Debug, "Pipeline: completion (remaining: %s) (%s modules in the pipeline)", (int)numRemainingNotifications, modules.size());
		condition.wait(lock);
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

void Pipeline::probe() {
	std::unique_lock<std::mutex> lock(mutex);
	++numRemainingNotifications;
	condition.notify_one();
}

void Pipeline::finished() {
	std::unique_lock<std::mutex> lock(mutex);
	assert(numRemainingNotifications > 0);
	--numRemainingNotifications;
	condition.notify_one();
}

}
