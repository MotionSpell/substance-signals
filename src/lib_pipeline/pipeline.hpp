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
	virtual bool isSink() = 0;
	virtual void connect(Modules::IOutput *output, int inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) = 0;
	virtual void disconnect(int inputIdx, Modules::IOutput * output) = 0;
};

struct IPipelineNotifier {
	virtual void endOfStream() = 0;
	virtual void exception(std::exception_ptr eptr) = 0;
};

class IPipeline {
	public:
		virtual ~IPipeline() = default;

		template <typename InstanceType, int NumBlocks = 0, typename ...Args>
		IPipelinedModule* addModule(Args&&... args) {
			return addModuleInternal(Modules::createModule<InstanceType>(getNumBlocks(NumBlocks), getClock(), std::forward<Args>(args)...));
		}

		// Remove a module from a pipeline.
		// This is only possible when the module is disconnected and flush()ed
		// (which is the caller responsibility - FIXME)
		virtual void removeModule(IPipelinedModule * module) = 0;

		virtual void connect(IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx, bool inputAcceptMultipleConnections = false) = 0;
		virtual void disconnect(IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx) = 0;

		virtual void start() = 0;
		virtual void waitForEndOfStream() = 0;
		virtual void exitSync() = 0; /*ask for all sources to finish*/

	protected:
		virtual IPipelinedModule* addModuleInternal(std::unique_ptr<Modules::IModule> rawModule) = 0;
		/*FIXME: the block below won't be necessary once we inject correctly*/
		virtual int getNumBlocks(int numBlock) const = 0;
		virtual std::shared_ptr<IClock> getClock() const = 0;
};

/* not thread-safe */
class Pipeline : public IPipeline, public IPipelineNotifier {
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

		IPipelinedModule* addModuleInternal(std::unique_ptr<Modules::IModule> rawModule) override;
		void removeModule(IPipelinedModule * module) override;
		void connect   (IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx, bool inputAcceptMultipleConnections = false) override;
		void disconnect(IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx) override;
		void start() override;
		void waitForEndOfStream() override;
		void exitSync() override;

		int getNumBlocks(int numBlock) const override {
			return numBlock ? numBlock : allocatorNumBlocks;
		}
		std::shared_ptr<IClock> getClock() const override {
			return clock;
		}

	private:
		void computeTopology();
		void endOfStream() override;
		void exception(std::exception_ptr eptr) override;

		std::vector<std::unique_ptr<IPipelinedModule>> modules;
		const int allocatorNumBlocks;
		const std::shared_ptr<IClock> clock;
		const Threading threading;

		std::mutex mutex;
		std::condition_variable condition;
		size_t notifications = 0;
		std::atomic_size_t remainingNotifications;
		std::exception_ptr eptr;
};

}
