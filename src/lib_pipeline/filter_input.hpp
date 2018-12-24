#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_utils/format.hpp"
#include <cstring>

namespace Pipelines {

/* Wrapper around the module's inputs.
   Data is queued in the calling thread, then always dispatched by the executor
   Data is nullptr at completion. */
class FilterInput : public IInput {
	public:
		FilterInput(IInput *input,
		    const std::string &moduleName,
		    Signals::IExecutor* executor,
		    IStatsRegistry* statsRegistry,
		    IPipelineNotifier * const notify,
		    KHost* host
		)
			: delegate(input), delegateName(moduleName), notify(notify), m_host(host), executor(executor),
			  statsCumulated(statsRegistry->getNewEntry()),
			  statsPending(statsRegistry->getNewEntry()) {

			strncpy(statsCumulated->name, (delegateName + ".cumulated").c_str(), sizeof(statsCumulated->name)-1);
			statsCumulated->name[sizeof(statsCumulated->name)-1] = 0;

			strncpy(statsPending->name, (delegateName + ".pending").c_str(), sizeof(statsPending->name)-1);
			statsPending->name[sizeof(statsPending->name)-1] = 0;
		}
		virtual ~FilterInput() {}

		void push(Data data) override {
			queue.push(data);
			statsPending->value ++;
		}

		Data pop() override {
			auto r = queue.pop();
			statsPending->value --;
			return r;
		}

		bool tryPop(Data& data) override {
			if(!queue.tryPop(data))
				return false;

			statsPending->value --;
			return true;
		}

		void process() override {
			auto data = pop();
			statsCumulated->value = samplingCounter++;

			auto doProcess = [this, data]() {
				try {
					// receiving 'nullptr' means 'end of stream'
					if (!data) {
						m_host->log(Debug, format("Module %s: notify end-of-stream.", delegateName).c_str());
						notify->endOfStream();
						return;
					}

					delegate->process();
				} catch(std::exception const& e) {
					m_host->log(Error, format("Can't process data: %s", e.what()).c_str());
					throw;
				}
			};

			if (data)
				delegate->push(data);

			executor->call(doProcess);
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
		KHost * const m_host;
		Signals::IExecutor * const executor;
		decltype(StatsEntry::value) samplingCounter = 0;
		decltype(StatsEntry::value) pendingCounter = 0;
		StatsEntry * const statsCumulated;
		StatsEntry * const statsPending;
		MetadataCap m_metadataCap;
};

}
