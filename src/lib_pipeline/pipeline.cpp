#include "stats.hpp"
#include "graph.hpp"
#include "pipelined_module.hpp"
#include "pipeline.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_utils/os.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <typeinfo>

#define COMPLETION_GRANULARITY_IN_MS 200

namespace Pipelines {

struct ModuleHost : Modules::IModuleHost {
	ModuleHost(std::string name_, LogSink* log_) : name(name_), m_log(log_) {
	}
	void log(int level, char const* msg) override {
		m_log->log((Level)level, format("[%s] %s", name.c_str(), msg).c_str());
	}
	const std::string name;
	LogSink* const m_log;
};


struct StatsRegistry : IStatsRegistry {
	StatsRegistry() : shmem(createSharedMemory(size, std::to_string(getPid()).c_str())), entryIdx(0) {
		memset(shmem->data(), 0, size);
	}

	StatsEntry* getNewEntry() override {
		entryIdx++;
		if (entryIdx >= maxNumEntry)
			throw std::runtime_error(format("SharedMemory: accessing too far (%s with max=%s).", entryIdx - 1, maxNumEntry - 1));

		return (StatsEntry*)shmem->data() + entryIdx - 1;
	}

	std::unique_ptr<SharedMemory> shmem;
	static const auto size = 256 * sizeof(StatsEntry);
	static const int maxNumEntry = size / sizeof(StatsEntry);
	StatsEntry **entries;
	int entryIdx;
};

std::unique_ptr<Modules::IModuleHost> Pipeline::createModuleHost(std::string name) {
	return make_unique<ModuleHost>(name, m_log);
}

Pipeline::Pipeline(LogSink* log, bool isLowLatency, Threading threading)
	: statsMem(new StatsRegistry), graph(new Graph),
	  m_log(log ? log : g_Log),
	  allocatorNumBlocks(isLowLatency ? Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY : Modules::ALLOC_NUM_BLOCKS_DEFAULT),
	  threading(threading) {
}

Pipeline::~Pipeline() {
}

IPipelinedModule* Pipeline::addModuleInternal(std::string name, std::unique_ptr<IModuleHost> host, std::shared_ptr<IModule> rawModule) {
	auto module = make_unique<PipelinedModule>(name.c_str(), move(host), rawModule, this, threading, statsMem.get());
	auto ret = module.get();
	modules.push_back(std::move(module));
	graph->nodes.push_back(Graph::Node{ret, name});
	return ret;
}

IPipelinedModule * Pipeline::add(char const* type, ...) {
	va_list va;
	va_start(va, type);
	auto name = format("%s (#%s)", type, (int)modules.size());
	auto host = createModuleHost(name);
	auto pHost = host.get();
	return addModuleInternal(type, std::move(host), vLoadModule(type, pHost, va));
}

void Pipeline::removeModule(IPipelinedModule *module) {
	auto findIf = [module](Pipelines::Graph::Connection const& c) {
		return c.src.id == module || c.dst.id == module;
	};
	auto i_conn = std::find_if(graph->connections.begin(), graph->connections.end(), findIf);
	if (i_conn != graph->connections.end())
		throw std::runtime_error("Could not remove module: connections found");

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

	computeTopology();
}

void Pipeline::connect(OutputPin prev, InputPin next, bool inputAcceptMultipleConnections) {
	if (!next.mod || !prev.mod) return;
	auto n = safe_cast<PipelinedModule>(next.mod);
	auto p = safe_cast<PipelinedModule>(prev.mod);

	{
		std::unique_lock<std::mutex> lock(remainingNotificationsMutex);
		if (remainingNotifications != notifications)
			throw std::runtime_error("Connection but the topology has changed. Not supported yet."); //TODO: to change that, we need to store a state of the PipelinedModule.
	}

	n->connect(p->getOutput(prev.index), next.index, inputAcceptMultipleConnections);
	computeTopology();

	graph->connections.push_back(Graph::Connection{graph->nodeFromId(prev.mod), prev.index, graph->nodeFromId(next.mod), next.index});
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

std::string Pipeline::dump() {
	std::stringstream ss;
	ss << "digraph {" << std::endl;
	ss << "\trankdir = \"LR\";" << std::endl;

	for (auto &node : graph->nodes)
		ss << "\t\"" << node.caption << "\";" << std::endl;

	for (auto &conn : graph->connections)
		ss << "\t\"" << conn.src.caption << "\" -> \"" << conn.dst.caption << "\";" << std::endl;

	ss << "}" << std::endl;
	return ss.str();
}

void Pipeline::start() {
	m_log->log(Info, "Pipeline: starting");
	computeTopology();
	for (auto &module : modules) {
		auto m = safe_cast<PipelinedModule>(module.get());
		if (m->isSource()) {
			m->startSource();
		}
	}
	m_log->log(Info, "Pipeline: started");
}

void Pipeline::waitForEndOfStream() {
	m_log->log(Info, "Pipeline: waiting for completion");
	std::unique_lock<std::mutex> lock(remainingNotificationsMutex);
	while (remainingNotifications > 0) {
		m_log->log(Debug, format("Pipeline: condition (remaining: %s) (%s modules in the pipeline)", remainingNotifications, modules.size()).c_str());
		condition.wait_for(lock, std::chrono::milliseconds(COMPLETION_GRANULARITY_IN_MS));
		try {
			if (eptr)
				std::rethrow_exception(eptr);
		} catch (const std::exception &e) {
			m_log->log(Error, format("Pipeline: exception caught: %s. Exiting.", e.what()).c_str());
			std::rethrow_exception(eptr); //FIXME: at this point the exception forward in submit() already lost some data
		}
	}
	m_log->log(Info, "Pipeline: completed");
}

void Pipeline::exitSync() {
	m_log->log(Info, "Pipeline: asked to exit now.");
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
			if (m->getInput(i)->isConnected())
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
