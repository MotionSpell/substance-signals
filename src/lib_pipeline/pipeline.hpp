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
	virtual void disconnect(size_t inputIdx, Modules::IOutput * const output) = 0;
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
		/*Remove a module from a pipeline. This is only possible when the module is already disconnected and flush()ed (which is the caller reponsibility - FIXME).*/
		void removeModule(IPipelinedModule * const module);

		void connect   (Modules::IModule * const prev, size_t outputIdx, Modules::IModule * const next, size_t inputIdx, bool inputAcceptMultipleConnections = false);
		void disconnect(Modules::IModule * const prev, size_t outputIdx, Modules::IModule * const next, size_t inputIdx);

		void start();
		void waitForCompletion();
		void exitSync(); /*ask for all sources to finish*/

	private:
		void computeTopology();
		void finished() override;
		void exception(std::exception_ptr eptr) override;
		IPipelinedModule* addModuleInternal(Modules::IModule * const rawModule);

		std::vector<std::unique_ptr<IPipelinedModule>> modules;
		const size_t allocatorNumBlocks;
		std::unique_ptr<const IClock> const clock;
		const Threading threading;

		std::mutex mutex;
		std::condition_variable condition;
		size_t notifications = 0;
		std::atomic_size_t remainingNotifications;
		std::exception_ptr eptr;
};

}
