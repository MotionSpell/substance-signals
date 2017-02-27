#include "pipelined_module.hpp"

#include "pipeline.hpp"
#include "lib_modules/utils/helper.hpp"

#define COMPLETION_GRANULARITY_IN_MS 200

namespace Pipelines {

Pipeline::Pipeline(bool isLowLatency, double clockSpeed, Threading threading)
: allocatorNumBlocks(isLowLatency ? Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY : Modules::ALLOC_NUM_BLOCKS_DEFAULT),
  clock(new Modules::Clock(clockSpeed)), threading(threading), numRemainingNotifications(0) {
}

IPipelinedModule* Pipeline::addModuleInternal(IModule *rawModule) {
	auto module = uptr(new PipelinedModule(rawModule, this, clock.get(), threading));
	auto ret = module.get();
	modules.push_back(std::move(module));
	return ret;
}

void Pipeline::connect(IModule *p, size_t outputIdx, IModule *n, size_t inputIdx, bool inputAcceptMultipleConnections) {
	if (!n || !p) return;
	auto next = safe_cast<IPipelinedModule>(n);
	auto prev = safe_cast<IPipelinedModule>(p);
	next->connect(prev->getOutput(outputIdx), inputIdx, prev->isSource(), inputAcceptMultipleConnections);
}

void Pipeline::start() {
	Log::msg(Info, "Pipeline: starting");
	computeTopology();
	for (auto &m : modules) {
		if (m->isSource()) {
			m->process();
		}
	}
	Log::msg(Info, "Pipeline: started");
}

void Pipeline::waitForCompletion() {
	Log::msg(Info, "Pipeline: waiting for completion");
	std::unique_lock<std::mutex> lock(mutex);
	while (numRemainingNotifications > 0) {
		Log::msg(Debug, "Pipeline: condition (remaining: %s) (%s modules in the pipeline)", (int)numRemainingNotifications, modules.size());
		condition.wait_for(lock, std::chrono::milliseconds(COMPLETION_GRANULARITY_IN_MS));
		try {
			if (eptr)
				std::rethrow_exception(eptr);
		} catch (const std::exception &e) {
			Log::msg(Error, "Pipeline: exception caught: %s. Exiting.", e.what());
			exitSync();
			std::rethrow_exception(eptr); //FIXME: at this point the exception forward in submit() already lost some data
		}
	}
	Log::msg(Info, "Pipeline: completed");
}

void Pipeline::exitSync() {
	Log::msg(Warning, "Pipeline: asked to exit now.");
	for (auto &m : modules) {
		if (m->isSource()) {
			m->process();
		}
	}
}

void Pipeline::computeTopology() {
	for (auto &m : modules) {
		if (m->isSink()) {
			if (m->isSource()) {
				numRemainingNotifications++;
			} else {
				for (size_t i = 0; i < m->getNumInputs(); ++i) {
					numRemainingNotifications += m->getInput(i)->getNumConnections();
				}
			}
		}
	}
}

void Pipeline::finished() {
	std::unique_lock<std::mutex> lock(mutex);
	assert(numRemainingNotifications > 0);
	--numRemainingNotifications;
	condition.notify_one();
}

void Pipeline::exception(std::exception_ptr e) {
	std::unique_lock<std::mutex> lock(mutex);
	eptr = e;
	condition.notify_one();
}

}
