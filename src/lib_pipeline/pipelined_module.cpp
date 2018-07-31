#include "lib_signals/executor_threadpool.hpp"
#include "stats.hpp"
#include "pipelined_module.hpp"
#include "pipelined_input.hpp"

#define EXECUTOR_SYNC              Signals::ExecutorSync
#define EXECUTOR_ASYNC_THREAD      Signals::ExecutorThread(m_name)

#define EXECUTOR                   EXECUTOR_ASYNC_THREAD
#define EXECUTOR_LIVE              EXECUTOR_SYNC
#define EXECUTOR_INPUT_DEFAULT     (&g_executorSync)

using namespace Modules;

namespace Pipelines {

/* take ownership of module and executor */
PipelinedModule::PipelinedModule(const char* name, std::unique_ptr<Modules::IModuleHost> host, std::shared_ptr<IModule> module, IPipelineNotifier *notify, Pipeline::Threading threading, IStatsRegistry *statsRegistry)
	: m_host(std::move(host)),
	  m_name(name),
	  delegate(std::move(module)),
	  executor(threading & Pipeline::Mono ? (IExecutor*)new EXECUTOR_LIVE : (IExecutor*)new EXECUTOR),
	  m_notify(notify),
	  eosCount(0),
	  statsRegistry(statsRegistry) {
}

PipelinedModule::~PipelinedModule() {
	// FIXME: we shouldn't do semantics here, but it is needed as long as
	//        the ActiveModule loop is not included in PipelinedModule
	if (isSource())
		stopSource();

	// inputs, which hold the executors,
	// must be destroyed *before* the 'delegate' module.
	inputs.clear();
}

int PipelinedModule::getNumInputs() const {
	return delegate->getNumInputs();
}
int PipelinedModule::getNumOutputs() const {
	return delegate->getNumOutputs();
}
IOutput* PipelinedModule::getOutput(int i) {
	if (i >= delegate->getNumOutputs())
		throw std::runtime_error(format("PipelinedModule %s: no output %s.", m_name, i));
	return delegate->getOutput(i);
}

std::shared_ptr<const IMetadata> PipelinedModule::getOutputMetadata(int i) {
	return getOutput(i)->getMetadata();
}

/* source modules are stopped manually - then the message propagates to other connected modules */
bool PipelinedModule::isSource() {
	return dynamic_cast<ActiveModule*>(delegate.get());
}

void PipelinedModule::connect(IOutput *output, int inputIdx, bool inputAcceptMultipleConnections) {
	auto input = getInput(inputIdx);
	ConnectOutputToInput(output, input, inputExecutor[inputIdx]);
	if (!inputAcceptMultipleConnections && (input->getNumConnections() != 1))
		throw std::runtime_error(format("PipelinedModule %s: input %s has %s connections.", m_name, inputIdx, input->getNumConnections()));
	connections++;
}

void PipelinedModule::disconnect(int inputIdx, IOutput * const output) {
	getInput(inputIdx)->disconnect();
	auto &sig = output->getSignal();
	auto const numConn = sig.getNumConnections();
	for (int i = 0; i < numConn; ++i) {
		sig.disconnect(i);
	}
	connections--;
}

void PipelinedModule::mimicInputs() {
	while ((int)inputs.size()< delegate->getNumInputs()) {
		auto const i = (int)inputs.size();
		inputExecutor.push_back(EXECUTOR_INPUT_DEFAULT);
		inputs.push_back(uptr(new PipelinedInput(delegate->getInput(i), m_name, *executor, statsRegistry->getNewEntry(), this)));
	}
}

IInput* PipelinedModule::getInput(int i) {
	mimicInputs();
	if (i >= (int)inputs.size())
		throw std::runtime_error(format("PipelinedModule %s: no input %s.", m_name, i));
	return inputs[i].get();
}

void PipelinedModule::startSource() {
	assert(isSource());

	if (started) {
		Log::msg(Info, "Pipeline: source already started . Doing nothing.");
		return;
	}

	connections = 1;

	auto task = [this]() {
		delegate->process();
		endOfStream();
	};

	(*executor)(task);

	started = true;
}

void PipelinedModule::stopSource() {
	assert(isSource());

	if (!started) {
		Log::msg(Warning, "Pipeline: cannot stop unstarted source. Ignoring.");
		return;
	}

	auto active = safe_cast<ActiveModule>(delegate.get());
	active->mustExit = true;
}

// IPipelineNotifier implementation

void PipelinedModule::endOfStream() {
	++eosCount;

	if (eosCount > connections) {
		auto const msg = format("PipelinedModule %s: received too many EOS (%s/%s)", m_name, (int)eosCount, (int)connections);
		throw std::runtime_error(msg);
	}

	if (eosCount == connections) {
		delegate->flush();

		for (int i = 0; i < delegate->getNumOutputs(); ++i) {
			delegate->getOutput(i)->emit(nullptr);
		}

		if (connections) {
			m_notify->endOfStream();
		}
	}
}

void PipelinedModule::exception(std::exception_ptr eptr) {
	m_notify->exception(eptr);
}

}
