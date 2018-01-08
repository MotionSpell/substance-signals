#pragma once

#include "lib_modules/modules.hpp"
#include <atomic>
#include <list>
#include <memory>
#include <stdexcept>

namespace Pipelines {

struct IPipelinedModule : public Modules::IModule {
	virtual bool isSource() = 0;
	virtual bool isSink() = 0;
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

class IPipeline {
public:
	template <typename InstanceType, int NumBlocks = 0, typename ...Args>
	IPipelinedModule* addModule(Args&&... args) {
		return addModuleInternal(Modules::createModule<InstanceType>(getNumBlocks(NumBlocks), getClock(), std::forward<Args>(args)...));
	}

	/*Remove a module from a pipeline. This is only possible when the module is already disconnected and flush()ed (which is the caller responsibility - FIXME).*/
	virtual void removeModule(IPipelinedModule * const module) = 0;

	virtual void connect(IPipelinedModule * const prev, size_t outputIdx, IPipelinedModule * const next, size_t inputIdx, bool inputAcceptMultipleConnections = false) = 0;
	virtual void disconnect(IPipelinedModule * const prev, size_t outputIdx, IPipelinedModule * const next, size_t inputIdx) = 0;

	virtual void start() = 0;
	virtual void waitForCompletion() = 0;
	virtual void exitSync() = 0;; /*ask for all sources to finish*/

protected:
	virtual IPipelinedModule* addModuleInternal(std::unique_ptr<Modules::IModule> rawModule) = 0;
	/*FIXME: the block below won't be necessary once we inject correctly*/
	virtual int getNumBlocks(int numBlock) const = 0;
	virtual const std::shared_ptr<IClock> getClock() const = 0;
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
	void removeModule(IPipelinedModule * const module);
	void connect   (IPipelinedModule * const prev, size_t outputIdx, IPipelinedModule * const next, size_t inputIdx, bool inputAcceptMultipleConnections = false);
	void disconnect(IPipelinedModule * const prev, size_t outputIdx, IPipelinedModule * const next, size_t inputIdx);
	void start();
	void waitForCompletion();
	void exitSync();

	const std::shared_ptr<IClock> getClock() const override {
		return clock;
	}

private:
	void computeTopology();
	void finished() override;
	void exception(std::exception_ptr eptr) override;
	int getNumBlocks(int numBlock) const override {
		return numBlock ? numBlock : allocatorNumBlocks;
	}

	std::list<std::unique_ptr<IPipelinedModule>> modules;
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
