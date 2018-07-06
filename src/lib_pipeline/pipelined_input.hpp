#pragma once

#include "pipeline.hpp"
#include "lib_modules/modules.hpp"

using namespace Modules;

namespace Pipelines {

/* Wrapper around the module's inputs.
   Data is queued in the calling thread, then always dispatched by the executor
   Data is nullptr at completion. */
class PipelinedInput : public IInput, public MetadataCap {
	public:
		PipelinedInput(IInput *input, const std::string &moduleName, IProcessExecutor &delegateExecutor, IPipelineNotifier * const notify)
			: delegate(input), delegateName(moduleName), notify(notify), delegateExecutor(delegateExecutor) {}
		virtual ~PipelinedInput() noexcept(false) {}

		void push(Data data) override {
			queue.push(data);
		}

		Data pop() override {
			return queue.pop();
		}

		bool tryPop(Data& data) override {
			return queue.tryPop(data);
		}

		void process() override {
			auto data = pop();

			// receiving 'nullptr' means 'end of stream'
			if (!data) {
				Log::msg(Debug, "Module %s: notify end-of-stream.", delegateName);
				delegateExecutor(Bind(&IPipelineNotifier::endOfStream, notify));
				return;
			}

			delegate->push(data);
			try {
				delegateExecutor(Bind(&IProcessor::process, delegate));
			} catch (...) { //stop now
				auto const &eptr = std::current_exception();
				notify->exception(eptr);
				std::rethrow_exception(eptr);
			}
		}

		int getNumConnections() const override {
			return delegate->getNumConnections();
		}
		void connect() override {
			delegate->connect();
		}
		void disconnect() override {
			delegate->disconnect();
		}

	private:
		Queue<Data> queue;
		IInput *delegate;
		std::string delegateName;
		IPipelineNotifier * const notify;
		IProcessExecutor &delegateExecutor;
};

}
