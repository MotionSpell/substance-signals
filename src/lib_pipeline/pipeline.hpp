#pragma once

#include "lib_modules/core/module.hpp"
#include <atomic>
#include <memory>
#include <stdexcept>
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

struct ICompletionNotifier {
	virtual void finished() = 0;
};

struct IExceptionNotifier {
	virtual void exception(std::exception_ptr eptr) = 0;
};

struct IPipelineNotifier : public ICompletionNotifier, public IExceptionNotifier {
};

/* not thread-safe */
class Pipeline : public IPipelineNotifier {
	public:
		enum Threading {
			Mono              = 1,
			OnePerModule      = 2,
			RegulationOffFlag = 1 << 10, //disable thread creation for each module connected from a source.
		};

		/* @isLowLatency Controls the default number of buffers.
		   @clockSpeed   Controls the execution speed (0.0 is "as fast as possible"): this may create threads.
		   @threading    Controls the threading. */
		Pipeline(bool isLowLatency = false, double clockSpeed = 0.0, Threading threading = OnePerModule);

		template <typename InstanceType, int NumBlocks = 0, typename ...Args>
		IPipelinedModule* addModule(Args&&... args) {
			return addModuleInternal(createModule<InstanceType>(NumBlocks ? NumBlocks : allocatorNumBlocks, std::forward<Args>(args)...));
		}

		void connect(Modules::IModule *prev, size_t outputIdx, Modules::IModule *next, size_t inputIdx, bool inputAcceptMultipleConnections = false);

		void start();
		void waitForCompletion();
		void exitSync(); /*ask for all sources to finish*/

	private:
		void computeTopology();
		void finished() override;
		void exception(std::exception_ptr eptr) override;
		IPipelinedModule* addModuleInternal(Modules::IModule *rawModule);

		std::vector<std::unique_ptr<IPipelinedModule>> modules;
		const size_t allocatorNumBlocks;
		std::unique_ptr<const Modules::IClock> const clock;
		Threading threading;

		std::mutex mutex;
		std::condition_variable condition;
		std::atomic_size_t numRemainingNotifications;
		std::exception_ptr eptr;
};

inline Pipeline::Threading operator | (Pipeline::Threading a, Pipeline::Threading b) {
	return static_cast<Pipeline::Threading>(static_cast<int>(a) | static_cast<int>(b));
}
inline Pipeline::Threading operator & (Pipeline::Threading a, Pipeline::Threading b) {
	return static_cast<Pipeline::Threading>(static_cast<int>(a) & static_cast<int>(b));
}

}
