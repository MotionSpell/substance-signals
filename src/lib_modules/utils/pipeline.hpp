#pragma once

#include "../core/module.hpp"
#include <atomic>
#include <memory>
#include <vector>


namespace Pipelines {

template <typename InstanceType, typename ...Args>
InstanceType* createModule(size_t allocatorSize, Args&&... args) {
	return new Modules::ModuleDefault<InstanceType>(allocatorSize, std::forward<Args>(args)...);
}

struct IPipelinedModule : public Modules::IModule {
	virtual bool isSource() const = 0;
	virtual bool isSink() const = 0;
	virtual void connect(Modules::IOutput *output, size_t inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) = 0;
};

struct ITopologyProber {
	virtual void probe() = 0;
};

struct ICompletionNotifier {
	virtual void finished() = 0;
};

struct IPipelineNotifier : public ICompletionNotifier, public ITopologyProber {
};

class Pipeline : public IPipelineNotifier {
	public:
		Pipeline(bool isLowLatency = false, double clockSpeed = 0.0);

		template <typename InstanceType, typename ...Args>
		IPipelinedModule* addModule(Args&&... args) {
			if (isLowLatency) {
				return addModuleInternal(createModule<InstanceType>(Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY, std::forward<Args>(args)...));
			} else {
				return addModuleInternal(createModule<InstanceType>(Modules::ALLOC_NUM_BLOCKS_DEFAULT, std::forward<Args>(args)...));
			}
		}

		void connect(Modules::IModule *prev, size_t outputIdx, Modules::IModule *next, size_t inputIdx, bool inputAcceptMultipleConnections = false);

		void start();
		void waitForCompletion();
		void exitSync(); /*ask for all sources to finish*/

	private:
		void finished() override;
		void probe() override;
		void startSources();
		void computeNotifications();
		IPipelinedModule* addModuleInternal(Modules::IModule *rawModule);

		std::vector<std::unique_ptr<IPipelinedModule>> modules;
		bool isLowLatency;
		std::unique_ptr<const Modules::IClock> const clock;

		std::mutex mutex;
		std::condition_variable condition;
		std::atomic_size_t numRemainingNotifications;
};

}
