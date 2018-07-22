#pragma once

#include "lib_modules/modules.hpp"
#include <atomic>
#include <vector>
#include <memory>
#include <stdexcept>

namespace Pipelines {

struct IPipelinedModule : public Modules::IModule {
	virtual void stopSource() = 0;
	virtual bool isSource() = 0;
	virtual void connect(Modules::IOutput *output, int inputIdx, bool inputAcceptMultipleConnections) = 0;
	virtual void disconnect(int inputIdx, Modules::IOutput * output) = 0;
};

struct IPipelineNotifier {
	virtual void endOfStream() = 0;
	virtual void exception(std::exception_ptr eptr) = 0;
};

/* not thread-safe */
class Pipeline : public IPipelineNotifier {
	public:
		enum Threading {
			Mono              = 1,
			OnePerModule      = 2,
		};

		template <typename InstanceType, int NumBlocks = 0, typename ...Args>
		IPipelinedModule * addModule(Args&&... args) {
			return addModuleInternal(Modules::createModule<InstanceType>(getNumBlocks(NumBlocks), nullptr, std::forward<Args>(args)...));
		}

		/* @isLowLatency Controls the default number of buffers.
			@threading    Controls the threading. */
		Pipeline(bool isLowLatency = false, Threading threading = OnePerModule);

		IPipelinedModule* addModuleInternal(std::unique_ptr<Modules::IModule> rawModule);

		// Remove a module from a pipeline.
		// This is only possible when the module is disconnected and flush()ed
		// (which is the caller responsibility - FIXME)
		void removeModule(IPipelinedModule * module);
		void connect   (IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx, bool inputAcceptMultipleConnections = false);
		void disconnect(IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx);
		void start();
		void waitForEndOfStream();
		void exitSync(); /*ask for all sources to finish*/

	private:
		void computeTopology();
		void endOfStream();
		void exception(std::exception_ptr eptr);

		/*FIXME: the block below won't be necessary once we inject correctly*/
		int getNumBlocks(int numBlock) const { //Romain: public?
			return numBlock ? numBlock : allocatorNumBlocks;
		}

		std::vector<std::unique_ptr<IPipelinedModule>> modules;
		const int allocatorNumBlocks;
		const Threading threading;

		std::mutex mutex;
		std::condition_variable condition;
		size_t notifications = 0;
		std::atomic_size_t remainingNotifications;
		std::exception_ptr eptr;
};

}
