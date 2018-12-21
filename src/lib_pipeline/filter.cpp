#include "filter.hpp"
#include "lib_signals/executor_threadpool.hpp"
#include "stats.hpp"
#include "filter_input.hpp"

using namespace Modules;
using Signals::IExecutor;

namespace Pipelines {

std::unique_ptr<IExecutor> createExecutor(Pipelines::Threading threading, const char* name) {
	if(int(threading & (Pipelines::Threading::Mono)))
		return make_unique<Signals::ExecutorSync>();
	else
		return make_unique<Signals::ExecutorThread>(name);
}

Filter::Filter(const char* name,
    LogSink* pLog,
    IPipelineNotifier *notify,
    Pipelines::Threading threading,
    IStatsRegistry *statsRegistry)
	: m_log(pLog),
	  m_name(name),
	  executor(createExecutor(threading, name)),
	  m_notify(notify),
	  eosCount(0),
	  statsRegistry(statsRegistry) {
}

// KHost implementation
void Filter::log(int level, char const* msg) {
	m_log->log((Level)level, format("[%s] %s", m_name.c_str(), msg).c_str());
}

void Filter::setDelegate(std::shared_ptr<IModule> module) {
	delegate = module;
}

int Filter::getNumInputs() const {
	return delegate->getNumInputs();
}

int Filter::getNumOutputs() const {
	return delegate->getNumOutputs();
}

IOutput* Filter::getOutput(int i) {
	if (i >= delegate->getNumOutputs())
		throw std::runtime_error(format("Filter %s: no output %s.", m_name, i));
	return delegate->getOutput(i);
}

Metadata Filter::getOutputMetadata(int i) {
	return getOutput(i)->getMetadata();
}

/* source modules are stopped manually - then the message propagates to other connected modules */
bool Filter::isSource() {
	return dynamic_cast<ActiveModule*>(delegate.get());
}

void Filter::connect(IOutput *output, int inputIdx, bool inputAcceptMultipleConnections) {
	auto input = getInput(inputIdx);
	if (!inputAcceptMultipleConnections && input->isConnected())
		throw std::runtime_error(format("Filter %s: input %s is already connected.", m_name, inputIdx));

	input->connect();

	CheckMetadataCompatibility(output, input);

	output->getSignal().connect([=](Data data) {
		input->push(data);
		input->process();
	});

	connections++;
}

void Filter::disconnect(int inputIdx, IOutput* output) {
	getInput(inputIdx)->disconnect();
	output->getSignal().disconnectAll();
	connections--;
}

void Filter::mimicInputs() {
	while ((int)inputs.size()< delegate->getNumInputs()) {
		auto idx = (int)inputs.size();
		auto dgInput = delegate->getInput(idx);
		auto name = format("%s, input (#%s)", m_name, idx);
		auto statEntry = statsRegistry->getNewEntry();
		inputs.push_back(uptr(new FilterInput(dgInput, name, executor.get(), statEntry, this)));
	}
}

IInput* Filter::getInput(int i) {
	mimicInputs();
	if (i >= (int)inputs.size())
		throw std::runtime_error(format("Filter %s: no input %s.", m_name, i));
	return inputs[i].get();
}

void Filter::processSource() {
	auto source = safe_cast<ActiveModule>(delegate.get());

	if(stopped || !source->work()) {
		endOfStream();
		return; // don't reschedule
	}

	// reschedule
	executor->call(std::bind(&Filter::processSource, this));
}

void Filter::startSource() {
	assert(isSource());

	if (started) {
		g_Log->log(Info, "Pipeline: source already started . Doing nothing.");
		return;
	}

	connections = 1;

	executor->call(std::bind(&Filter::processSource, this));

	started = true;
}

void Filter::stopSource() {
	assert(isSource());

	if (!started) {
		g_Log->log(Warning, "Pipeline: cannot stop unstarted source. Ignoring.");
		return;
	}

	stopped = true;
}

// IPipelineNotifier implementation

void Filter::endOfStream() {
	++eosCount;

	if (eosCount > connections) {
		auto const msg = format("Filter %s: received too many EOS (%s/%s)", m_name, (int)eosCount, (int)connections);
		throw std::runtime_error(msg);
	}

	if (eosCount == connections) {
		delegate->flush();

		for (int i = 0; i < delegate->getNumOutputs(); ++i) {
			delegate->getOutput(i)->post(nullptr);
		}

		if (connections) {
			m_notify->endOfStream();
		}
	}
}

void Filter::exception(std::exception_ptr eptr) {
	m_notify->exception(eptr);
}

}
