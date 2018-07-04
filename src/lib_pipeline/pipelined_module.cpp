#include "pipelined_module.hpp"
#include "pipelined_input.hpp"

using namespace Modules;

/* automatic inputs have a loose datatype */
struct DataLoosePipeline : public DataBase {};

namespace Pipelines {

/* take ownership of module and executor */
PipelinedModule::PipelinedModule(std::unique_ptr<IModule> module, IPipelineNotifier *notify, const std::shared_ptr<IClock> clock, Pipeline::Threading threading)
	: ClockCap(clock),
	  delegate(std::move(module)), localDelegateExecutor(threading & Pipeline::Mono ? (IProcessExecutor*)new EXECUTOR_LIVE : (IProcessExecutor*)new EXECUTOR),
	  delegateExecutor(*localDelegateExecutor), threading(threading), m_notify(notify), eosCount(0) {
}

PipelinedModule::~PipelinedModule() {
	// inputs, which hold the executors,
	// must be destroyed *before* the 'delegate' module.
	inputs.clear();
}

std::string PipelinedModule::getDelegateName() const {
	auto const &dref = *delegate.get();
	return typeid(dref).name();
}

int PipelinedModule::getNumInputs() const {
	return delegate->getNumInputs();
}
int PipelinedModule::getNumOutputs() const {
	return delegate->getNumOutputs();
}
IOutput* PipelinedModule::getOutput(int i) {
	if (i >= delegate->getNumOutputs())
		throw std::runtime_error(format("PipelinedModule %s: no output %s.", getDelegateName(), i));
	return delegate->getOutput(i);
}

/* source modules are stopped manually - then the message propagates to other connected modules */
bool PipelinedModule::isSource() {
	if (delegate->getNumInputs() == 0) {
		return true;
	} else if (delegate->getNumInputs() == 1 && dynamic_cast<Input<DataLoosePipeline>*>(delegate->getInput(0))) {
		return true;
	} else {
		return false;
	}
}
bool PipelinedModule::isSink() {
	for (int i = 0; i < getNumOutputs(); ++i) {
		if (getOutput(i)->getSignal().getNumConnections() > 0)
			return false;
	}
	return true;
}

void PipelinedModule::connect(IOutput *output, int inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) {
	auto input = safe_cast<PipelinedInput>(getInput(inputIdx));
	if (forceAsync && !(threading & Pipeline::RegulationOffFlag) && (inputExecutor[inputIdx] == EXECUTOR_INPUT_DEFAULT)) {
		auto executor = uptr(new REGULATION_EXECUTOR);
		inputExecutor[inputIdx] = executor.get();
		input->setLocalExecutor(std::move(executor));
	}
	ConnectOutputToInput(output, input, inputExecutor[inputIdx]);
	if (!inputAcceptMultipleConnections && (input->getNumConnections() != 1))
		throw std::runtime_error(format("PipelinedModule %s: input %s has %s connections.", getDelegateName(), inputIdx, input->getNumConnections()));
	connections++;
}

void PipelinedModule::disconnect(int inputIdx, IOutput * const output) {
	getInput(inputIdx)->disconnect();
	auto &sig = output->getSignal();
	auto const numConn = sig.getNumConnections();
	for (size_t i = 0; i < numConn; ++i) {
		sig.disconnect(i);
	}
	connections--;
}

void PipelinedModule::mimicInputs() {
	while ((int)inputs.size()< delegate->getNumInputs()) {
		auto const i = (int)inputs.size();
		inputExecutor.push_back(EXECUTOR_INPUT_DEFAULT);
		addInput(new PipelinedInput(delegate->getInput(i), getDelegateName(), inputExecutor[i], this->delegateExecutor, this, clock));
	}
}

IInput* PipelinedModule::getInput(int i) {
	mimicInputs();
	if (i >= (int)inputs.size())
		throw std::runtime_error(format("PipelinedModule %s: no input %s.", getDelegateName(), i));
	return inputs[i].get();
}


void PipelinedModule::stopSource() {
	assert(isSource());

	// the source is likely processing: push EOS in the loop
	// and let things follow their way*/
	delegate->getInput(0)->push(nullptr);
}

/* uses the executor (i.e. may defer the call) */
void PipelinedModule::process() {
	assert(isSource());

	Log::msg(Debug, "Module %s: dispatch data", getDelegateName());

	// first time: create a fake input port
	// and push null to trigger execution
	safe_cast<InputCap>(delegate.get())->addInput(new Input<DataLoosePipeline>(delegate.get()));
	connections = 1;
	getInput(0)->push(nullptr);
	delegate->getInput(0)->push(nullptr);
	delegateExecutor(Bind(&IProcessor::process, delegate.get()));
	delegateExecutor(Bind(&IProcessor::process, getInput(0)));
}

// IPipelineNotifier implementation

void PipelinedModule::endOfStream() {
	++eosCount;

	if (eosCount > connections) {
		auto const msg = format("PipelinedModule %s: received too many EOS (%s/%s)", getDelegateName(), (int)eosCount, (int)connections);
		throw std::runtime_error(msg);
	}

	if (eosCount == connections) {
		delegate->flush();

		for (int i = 0; i < delegate->getNumOutputs(); ++i) {
			delegate->getOutput(i)->emit(nullptr);
		}

		if (isSink()) {
			if (connections) {
				m_notify->endOfStream();
			}
		}
	}
}

void PipelinedModule::exception(std::exception_ptr eptr) {
	m_notify->exception(eptr);
}


}
