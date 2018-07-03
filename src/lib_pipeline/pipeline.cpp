#include <algorithm>
#include "pipelined_module.hpp"
#include "pipeline.hpp"
#include "lib_utils/sysclock.hpp"
#include "lib_modules/utils/helper.hpp"

#define COMPLETION_GRANULARITY_IN_MS 200

namespace Pipelines {

Pipeline::Pipeline(bool isLowLatency, double clockSpeed, Threading threading)
	: allocatorNumBlocks(isLowLatency ? Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY : Modules::ALLOC_NUM_BLOCKS_DEFAULT),
	  clock(new SystemClock(clockSpeed)), threading(threading), remainingNotifications(0) {
}

IPipelinedModule* Pipeline::addModuleInternal(std::unique_ptr<IModule> rawModule) {
	auto module = make_unique<PipelinedModule>(std::move(rawModule), this, clock, threading);
	auto ret = module.get();
	modules.push_back(std::move(module));
	return ret;
}

void Pipeline::removeModule(IPipelinedModule *module) {
	auto removeIt = [module](std::unique_ptr<IPipelinedModule> const& m) {
		return m.get() == module;
	};

	std::unique_lock<std::mutex> lock(mutex);
	auto i_mod = std::find_if(modules.begin(), modules.end(), removeIt);
	if(i_mod == modules.end())
		throw std::runtime_error("Could not remove module from pipeline");

	modules.erase(i_mod);
}

void Pipeline::connect(IPipelinedModule * const prev, int outputIdx, IPipelinedModule * const next, int inputIdx, bool inputAcceptMultipleConnections) {
	if (!next || !prev) return;
	std::unique_lock<std::mutex> lock(mutex);
	if (remainingNotifications != notifications)
		throw std::runtime_error("Connection but the topology has changed. Not supported yet."); //TODO: to change that, we need to store a state of the PipelinedModule.
	next->connect(prev->getOutput(outputIdx), inputIdx, prev->isSource(), inputAcceptMultipleConnections);
	computeTopology();
}

void Pipeline::disconnect(IPipelinedModule * const prev, int outputIdx, IPipelinedModule * const next, int inputIdx) {
	if (!prev) return;
	std::unique_lock<std::mutex> lock(mutex);
	if (remainingNotifications != notifications)
		throw std::runtime_error("Disconnection but the topology has changed. Not supported yet.");
	next->disconnect(inputIdx, prev->getOutput(outputIdx));
	computeTopology();
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

void Pipeline::waitForEndOfStream() {
	Log::msg(Info, "Pipeline: waiting for completion");
	std::unique_lock<std::mutex> lock(mutex);
	while (remainingNotifications > 0) {
		Log::msg(Debug, "Pipeline: condition (remaining: %s) (%s modules in the pipeline)", (int)remainingNotifications, modules.size());
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
			m->process(); //FIXME: ok but then we don't flush() so some module may stuck in waitForEndOfStream
		}
	}
}

void Pipeline::computeTopology() {
	notifications = 0;
	for (auto &m : modules) {
		if (m->isSink()) {
			if (m->isSource()) {
				notifications++;
			} else {
				for (int i = 0; i < m->getNumInputs(); ++i) {
					if (m->getInput(i)->getNumConnections()) {
						notifications++;
						break;
					}
				}
			}
		}
	}
	remainingNotifications = notifications;
}

void Pipeline::endOfStream() {
	std::unique_lock<std::mutex> lock(mutex);
	assert(remainingNotifications > 0);
	--remainingNotifications;
	condition.notify_one();
}

void Pipeline::exception(std::exception_ptr e) {
	std::unique_lock<std::mutex> lock(mutex);
	eptr = e;
	condition.notify_one();
}

}
