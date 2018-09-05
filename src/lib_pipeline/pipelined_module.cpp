#include "lib_signals/executor_threadpool.hpp"
#include "stats.hpp"
#include "pipelined_module.hpp"
#include "pipelined_input.hpp"

#define EXECUTOR_SYNC              Signals::ExecutorSync
#define EXECUTOR_ASYNC_THREAD      Signals::ExecutorThread(m_name)

#define EXECUTOR                   EXECUTOR_ASYNC_THREAD
#define EXECUTOR_LIVE              EXECUTOR_SYNC

using namespace Modules;

namespace Pipelines {

PipelinedModule::PipelinedModule(const char* name,
    std::unique_ptr<Modules::IModuleHost> host,
    std::shared_ptr<IModule> module,
    IPipelineNotifier *notify,
    Pipeline::Threading threading,
    IStatsRegistry *statsRegistry)
	: m_host(std::move(host)),
	  m_name(name),
	  delegate(std::move(module)),
	  executor(threading & Pipeline::Mono ? (IExecutor*)new EXECUTOR_LIVE : (IExecutor*)new EXECUTOR),
	  m_notify(notify),
	  eosCount(0),
	  statsRegistry(statsRegistry) {
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
	if (!inputAcceptMultipleConnections && input->isConnected())
		throw std::runtime_error(format("PipelinedModule %s: input %s is already connected.", m_name, inputIdx));
	ConnectOutputToInput(output, input);
	connections++;
}

void PipelinedModule::disconnect(int inputIdx, IOutput* output) {
	getInput(inputIdx)->disconnect();
	output->getSignal().disconnectAll();
	connections--;
}

void PipelinedModule::mimicInputs() {
	while ((int)inputs.size()< delegate->getNumInputs()) {
		auto dgInput = delegate->getInput((int)inputs.size());
		inputs.push_back(uptr(new PipelinedInput(dgInput, m_name, *executor, statsRegistry->getNewEntry(), this)));
	}
}

IInput* PipelinedModule::getInput(int i) {
	mimicInputs();
	if (i >= (int)inputs.size())
		throw std::runtime_error(format("PipelinedModule %s: no input %s.", m_name, i));
	return inputs[i].get();
}

void PipelinedModule::processSource() {
	auto source = safe_cast<ActiveModule>(delegate.get());

	if(stopped || !source->work()) {
		endOfStream();
		return; // don't reschedule
	}

	// reschedule
	(*executor)(std::bind(&PipelinedModule::processSource, this));
}

void PipelinedModule::startSource() {
	assert(isSource());

	if (started) {
		g_Log->log(Info, "Pipeline: source already started . Doing nothing.");
		return;
	}

	connections = 1;

	(*executor)(std::bind(&PipelinedModule::processSource, this));

	started = true;
}

void PipelinedModule::stopSource() {
	assert(isSource());

	if (!started) {
		g_Log->log(Warning, "Pipeline: cannot stop unstarted source. Ignoring.");
		return;
	}

	stopped = true;
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
