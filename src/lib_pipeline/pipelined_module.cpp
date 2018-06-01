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

std::string PipelinedModule::getDelegateName() const {
	auto const &dref = *delegate.get();
	return typeid(dref).name();
}

size_t PipelinedModule::getNumInputs() const {
	return delegate->getNumInputs();
}
size_t PipelinedModule::getNumOutputs() const {
	return delegate->getNumOutputs();
}
IOutput* PipelinedModule::getOutput(size_t i) {
	if (i >= delegate->getNumOutputs())
		throw std::runtime_error(format("PipelinedModule %s: no output %s.", getDelegateName(), i));
	return delegate->getOutput(i);
}

/* source modules are stopped manually - then the message propagates to other connected modules */
bool PipelinedModule::isSource() {
	if (delegate->getNumInputs() == 0) {
		return true;
	} else if (delegate->getNumInputs() == 1 && dynamic_cast<Input<DataLoosePipeline, IProcessor>*>(delegate->getInput(0))) {
		return true;
	} else {
		return false;
	}
}
bool PipelinedModule::isSink() {
	for (size_t i = 0; i < getNumOutputs(); ++i) {
		if (getOutput(i)->getSignal().getNumConnections() > 0)
			return false;
	}
	return true;
}

void PipelinedModule::connect(IOutput *output, size_t inputIdx, bool forceAsync, bool inputAcceptMultipleConnections) {
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

void PipelinedModule::disconnect(size_t inputIdx, IOutput * const output) {
	getInput(inputIdx)->disconnect();
	auto &sig = output->getSignal();
	auto const numConn = sig.getNumConnections();
	for (size_t i = 0; i < numConn; ++i) {
		sig.disconnect(i);
	}
	connections--;
}

void PipelinedModule::mimicInputs() {
	auto const delegateInputs = delegate->getNumInputs();
	auto const thisInputs = inputs.size();
	if (thisInputs < delegateInputs) {
		for (size_t i = thisInputs; i < delegateInputs; ++i) {
			inputExecutor.push_back(EXECUTOR_INPUT_DEFAULT);
			addInput(new PipelinedInput(delegate->getInput(i), getDelegateName(), inputExecutor[i], this->delegateExecutor, this, clock));
		}
	}
}

IInput* PipelinedModule::getInput(size_t i) {
	mimicInputs();
	if (i >= inputs.size())
		throw std::runtime_error(format("PipelinedModule %s: no input %s.", getDelegateName(), i));
	return inputs[i].get();
}

/* uses the executor (i.e. may defer the call) */
void PipelinedModule::process() {
	Log::msg(Debug, "Module %s: dispatch data", getDelegateName());

	assert(isSource());
	if (getNumInputs() == 0) { /*first time: create a fake input port and push null to trigger execution*/
		safe_cast<InputCap>(delegate.get())->addInput(new Input<DataLoosePipeline>(delegate.get()));
		connections = 1;
		getInput(0)->push(nullptr);
		delegate->getInput(0)->push(nullptr);
		delegateExecutor(Bind(&IProcessor::process, delegate.get()));
		delegateExecutor(Bind(&IProcessor::process, getInput(0)));
	} else { /*the source is likely processing: push null in the loop to exit and let things follow their way*/
		delegate->getInput(0)->push(nullptr);
	}
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

		for (size_t i = 0; i < delegate->getNumOutputs(); ++i) {
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
