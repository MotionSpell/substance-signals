#pragma once

#include "pipeline.hpp"
#include "lib_modules/modules.hpp"
#include <cstring>

using namespace Modules;

namespace Pipelines {

/* Wrapper around the module's inputs.
   Data is queued in the calling thread, then always dispatched by the executor
   Data is nullptr at completion. */
class PipelinedInput : public IInput, public MetadataCap {
	public:
		PipelinedInput(IInput *input, const std::string &moduleName, IExecutor &executor, StatsEntry *statsEntry, IPipelineNotifier * const notify)
			: delegate(input), delegateName(moduleName), notify(notify), executor(executor), statsEntry(statsEntry) {
			strncpy(statsEntry->name, delegateName.c_str(), sizeof(statsEntry->name)-1);
			statsEntry->name[sizeof(statsEntry->name)-1] = 0;
		}
		virtual ~PipelinedInput() {}

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
			statsEntry->value = samplingCounter++;

			// receiving 'nullptr' means 'end of stream'
			if (!data) {
				g_Log->log(Debug, format("Module %s: notify end-of-stream.", delegateName).c_str());
				executor(Bind(&IPipelineNotifier::endOfStream, notify));
				return;
			}

			delegate->push(data);
			try {
				executor(Bind(&IProcessor::process, delegate));
			} catch (...) { //stop now
				auto const &eptr = std::current_exception();
				notify->exception(eptr);
				std::rethrow_exception(eptr);
			}
		}

		int isConnected() const override {
			return delegate->isConnected();
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
		IExecutor &executor;
		decltype(StatsEntry::value) samplingCounter = 0;
		StatsEntry * const statsEntry;
};

}
