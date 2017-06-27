#pragma once

#include "lib_modules/modules.hpp"
#include <atomic>
#include <memory>
#include <stdexcept>
#include <vector>


namespace Pipelines {

struct IPipelinedModule : public Modules::IModule {
	virtual bool isSource() const = 0;
	virtual bool isSink() const = 0;
	virtual void connect(Modules::IOutput *output, size_t inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) = 0;
	virtual void disconnectAll(Modules::IOutput *output) = 0;
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
			return addModuleInternal(Modules::createModule<InstanceType>(NumBlocks ? NumBlocks : allocatorNumBlocks, std::forward<Args>(args)...));
		}

		void connect   (Modules::IModule *prev, size_t outputIdx, Modules::IModule *next, size_t inputIdx, bool inputAcceptMultipleConnections = false);
		void disconnect(Modules::IModule *prev, size_t outputIdx);

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
		const Threading threading;

		std::mutex mutex;
		std::condition_variable condition;
		size_t notifications;
		std::atomic_size_t remainingNotifications;
		std::exception_ptr eptr;
};

}
