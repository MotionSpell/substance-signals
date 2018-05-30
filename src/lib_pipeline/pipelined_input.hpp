#pragma once

#include "pipeline.hpp"
#include "lib_modules/modules.hpp"

#define EXECUTOR_SYNC              Signals::ExecutorSync<void()>
#define EXECUTOR_ASYNC_THREAD      Signals::ExecutorThread<void()>(getDelegateName())

#define EXECUTOR                   EXECUTOR_ASYNC_THREAD
#define EXECUTOR_LIVE              EXECUTOR_SYNC
#define EXECUTOR_INPUT_DEFAULT     (&g_executorSync)

#define REGULATION_EXECUTOR        EXECUTOR_ASYNC_THREAD
#define REGULATION_TOLERANCE_IN_MS 300

using namespace Modules;

namespace Pipelines {

/* Wrapper around the module's inputs.
   Data is queued in the calling thread, then always dispatched by the executor
       (with a delay if the clock is set: this assumes the connection is made
       using an asynchronous executor and the start time is zero).
   Data is nullptr at completion. */
class PipelinedInput : public IInput, public MetadataCap {
	public:
		PipelinedInput(IInput *input, const std::string &moduleName, IProcessExecutor *localExecutor, IProcessExecutor &delegateExecutor, IPipelineNotifier * const notify, const std::shared_ptr<IClock> clock)
			: delegate(input), delegateName(moduleName), notify(notify), executor(localExecutor), delegateExecutor(delegateExecutor), clock(clock) {}
		virtual ~PipelinedInput() noexcept(false) {}

		void process() override {
			auto data = pop();

			// receiving 'nullptr' means 'end of stream'
			if (!data) {
				Log::msg(Debug, "Module %s: notify end-of-stream.", delegateName);
				delegateExecutor(Bind(&IPipelineNotifier::endOfStream, notify));
				return;
			}

			auto const dataTime = data->getMediaTime();
			if (!dynamic_cast<EXECUTOR_SYNC*>(executor)) {
				regulate(dataTime);
			}
			Log::msg(Debug, "Module %s: dispatch data for time %ss", delegateName, dataTime / (double)IClock::Rate);
			delegate->push(data);
			try {
				delegateExecutor(Bind(&IProcessor::process, delegate));
			} catch (...) { //stop now
				auto const &eptr = std::current_exception();
				notify->exception(eptr);
				std::rethrow_exception(eptr);
			}
		}

		size_t getNumConnections() const override {
			return delegate->getNumConnections();
		}
		void connect() override {
			delegate->connect();
		}
		void disconnect() override {
			delegate->disconnect();
		}

		void setLocalExecutor(std::unique_ptr<IProcessExecutor> e) {
			localExecutor = std::move(e);
			executor = localExecutor.get();
		}

	private:
		void regulate(int64_t dataTime) {
			if (clock->getSpeed() > 0.0) {
				auto const delayInMs = clockToTimescale(dataTime - fractionToClock(clock->now()), 1000);
				if (delayInMs > 0) {
					Log::msg(delayInMs < REGULATION_TOLERANCE_IN_MS ? Debug : Warning, "Module %s: received data for time %ss (will sleep for %s ms)", delegateName, dataTime / (double)IClock::Rate, delayInMs);
					clock->sleep(Fraction(delayInMs, 1000));
				} else if (delayInMs + REGULATION_TOLERANCE_IN_MS < 0) {
					Log::msg(dataTime > 0 ? Warning : Debug, "Module %s: received data for time %ss is late from %sms", delegateName, dataTime / (double)IClock::Rate, -delayInMs);
				}
			}
		}

		IInput *delegate;
		std::string delegateName;
		IPipelineNotifier * const notify;
		IProcessExecutor *executor, &delegateExecutor;
		std::unique_ptr<IProcessExecutor> localExecutor;
		const std::shared_ptr<IClock> clock;
};

}
