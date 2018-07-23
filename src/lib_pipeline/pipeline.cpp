#include "graph.hpp"
#include <algorithm>
#include "pipelined_module.hpp"
#include "pipeline.hpp"
#include "lib_modules/utils/helper.hpp"

#define COMPLETION_GRANULARITY_IN_MS 200

namespace Pipelines {

Pipeline::Pipeline(bool isLowLatency, Threading threading)
	: graph(new Graph),
	  allocatorNumBlocks(isLowLatency ? Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY : Modules::ALLOC_NUM_BLOCKS_DEFAULT),
	  threading(threading) {
}

Pipeline::~Pipeline() {
}

IPipelinedModule* Pipeline::addModuleInternal(std::unique_ptr<IModule> rawModule) {
	auto module = make_unique<PipelinedModule>(std::move(rawModule), this, threading);
	auto ret = module.get();
	modules.push_back(std::move(module));
	graph->nodes.push_back(Graph::Node(ret));
	return ret;
}

void Pipeline::removeModule(IPipelinedModule *module) {
	auto findIf = [module](Pipelines::Graph::Connection const& c) {
		return c.src.id == module || c.dst.id == module;
	};
	auto i_conn = std::find_if(graph->connections.begin(), graph->connections.end(), findIf);
	if (i_conn != graph->connections.end())
		throw std::runtime_error("Could not remove module: conenctions found");

	auto removeIt = [module](std::unique_ptr<IPipelinedModule> const& m) {
		return m.get() == module;
	};
	auto i_mod = std::find_if(modules.begin(), modules.end(), removeIt);
	if (i_mod == modules.end())
		throw std::runtime_error("Could not remove from pipeline: module not found");
	modules.erase(i_mod);

	auto i_node = std::find_if(graph->nodes.begin(), graph->nodes.end(), [module](Pipelines::Graph::Node const& n) {
		return n.id == module;
	});
	assert(i_node != graph->nodes.end());
	graph->nodes.erase(i_node);
}

void Pipeline::connect(IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx, bool inputAcceptMultipleConnections) {
	if (!next || !prev) return;
	auto n = safe_cast<PipelinedModule>(next);
	auto p = safe_cast<PipelinedModule>(prev);

	{
		std::unique_lock<std::mutex> lock(remainingNotificationsMutex);
		if (remainingNotifications != notifications)
			throw std::runtime_error("Connection but the topology has changed. Not supported yet."); //TODO: to change that, we need to store a state of the PipelinedModule.
	}

	n->connect(p->getOutput(outputIdx), inputIdx, inputAcceptMultipleConnections);
	computeTopology();

	graph->connections.push_back(Graph::Connection(graph->nodeFromId(prev), outputIdx, graph->nodeFromId(next), inputIdx));
}

void Pipeline::disconnect(IPipelinedModule * prev, int outputIdx, IPipelinedModule * next, int inputIdx) {
	if (!prev) return;
	auto n = safe_cast<PipelinedModule>(next);
	auto p = safe_cast<PipelinedModule>(prev);

	auto removeIf = [prev, outputIdx, next, inputIdx](Pipelines::Graph::Connection const& c) {
		return c.src.id == prev && c.srcPort == outputIdx && c.dst.id == next && c.dstPort == inputIdx;
	};
	auto i_conn = std::find_if(graph->connections.begin(), graph->connections.end(), removeIf);
	if (i_conn == graph->connections.end())
		throw std::runtime_error("Could not disconnect: connection not found");
	graph->connections.erase(i_conn);

	{
		std::unique_lock<std::mutex> lock(remainingNotificationsMutex);
		if (remainingNotifications != notifications)
			throw std::runtime_error("Disconnection but the topology has changed. Not supported yet.");
	}
	n->disconnect(inputIdx, p->getOutput(outputIdx));
	computeTopology();
}

std::stringstream Pipeline::dump() {
	std::stringstream ss;
	ss << "graph {" << std::endl;

	int idx = 0;
	for (auto &node : graph->nodes) {
		ss << "\t" << "subgraph cluster_" << idx++ << " {" << std::endl;
		ss << "\t\tlabel = \"" << node.id << "\";" << std::endl;

		ss << "\t\t" << "subgraph cluster_inputs {" << std::endl;
		ss << "\t\t\tlabel = \"inputs\";" << std::endl;
		for (int i = 0; i < node.id->getNumInputs(); ++i) {
			ss << "\t\t\t\"" << node.id << "_input_" << i << "\";" << std::endl;
		}
		ss << "\t\t" << "}" << std::endl;

		ss << "\t\t" << "subgraph cluster_outputs {" << std::endl;
		ss << "\t\t\tlabel = \"outputs\";" << std::endl;
		for (int i = 0; i < node.id->getNumOutputs(); ++i) {
			ss << "\t\t\t\"" << node.id << "_output_" << i << "\";" << std::endl;
		}
		ss << "\t\t" << "}" << std::endl;

		ss << "\t" << "}" << std::endl << std::endl;
	}

	for (auto &conn : graph->connections) {
		ss << "\t\"" << conn.src.id << "_output_" << conn.srcPort << "\" -> \"" << conn.dst.id << "_input_" << conn.dstPort << "\";" << std::endl;
	}

	ss << "}" << std::endl;
	return ss;
}

void Pipeline::start() {
	Log::msg(Info, "Pipeline: starting");
	computeTopology();
	for (auto &module : modules) {
		auto m = safe_cast<PipelinedModule>(module.get());
		if (m->isSource()) {
			m->process();
		}
	}
	Log::msg(Info, "Pipeline: started");
}

void Pipeline::waitForEndOfStream() {
	Log::msg(Info, "Pipeline: waiting for completion");
	std::unique_lock<std::mutex> lock(remainingNotificationsMutex);
	while (remainingNotifications > 0) {
		Log::msg(Debug, "Pipeline: condition (remaining: %s) (%s modules in the pipeline)", remainingNotifications, modules.size());
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
	for (auto &module : modules) {
		auto m = safe_cast<PipelinedModule>(module.get());
		if (m->isSource()) {
			m->stopSource();
		}
	}
}

void Pipeline::computeTopology() {
	auto hasAtLeastOneInputConnected = [](PipelinedModule* m) {
		for (int i = 0; i < m->getNumInputs(); ++i) {
			if (m->getInput(i)->getNumConnections())
				return true;
		}
		return false;
	};

	notifications = 0;
	for (auto &module : modules) {
		auto m = safe_cast<PipelinedModule>(module.get());
		if (m->isSource() || hasAtLeastOneInputConnected(m))
			notifications++;
	}

	std::unique_lock<std::mutex> lock(remainingNotificationsMutex);
	remainingNotifications = notifications;
}

void Pipeline::endOfStream() {
	{
		std::unique_lock<std::mutex> lock(remainingNotificationsMutex);
		assert(remainingNotifications > 0);
		--remainingNotifications;
	}

	condition.notify_one();
}

void Pipeline::exception(std::exception_ptr e) {
	eptr = e;
	condition.notify_one();
}

}
