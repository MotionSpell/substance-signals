#pragma once

#include "pipeline.hpp"
#include "lib_modules/modules.hpp"
#include <cstring>

using namespace Modules;

namespace Pipelines {

/* Wrapper around the module's inputs.
   Data is queued in the calling thread, then always dispatched by the executor
   Data is nullptr at completion. */
class FilterInput : public IInput {
	public:
		FilterInput(IInput *input, const std::string &moduleName, Signals::IExecutor* executor, StatsEntry *statsEntry, IPipelineNotifier * const notify)
			: delegate(input), delegateName(moduleName), notify(notify), executor(executor), statsEntry(statsEntry) {
			strncpy(statsEntry->name, delegateName.c_str(), sizeof(statsEntry->name)-1);
			statsEntry->name[sizeof(statsEntry->name)-1] = 0;
		}
		virtual ~FilterInput() {}

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
				executor->call(Signals::Bind(&IPipelineNotifier::endOfStream, notify));
				return;
			}

			delegate->push(data);
			executor->call(Signals::Bind(&IProcessor::process, delegate));
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

		Metadata getMetadata() const override {
			return m_metadataCap.getMetadata();
		}
		void setMetadata(Metadata metadata) override {
			m_metadataCap.setMetadata(metadata);
		}
		bool updateMetadata(Data &data) override {
			return m_metadataCap.updateMetadata(data);
		}

	private:
		Queue<Data> queue;
		IInput *delegate;
		std::string delegateName;
		IPipelineNotifier * const notify;
		Signals::IExecutor * const executor;
		decltype(StatsEntry::value) samplingCounter = 0;
		StatsEntry * const statsEntry;
		MetadataCap m_metadataCap;
};

}
