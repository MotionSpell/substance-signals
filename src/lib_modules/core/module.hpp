#pragma once

// This is how the framework sees custom module implementations.
// This defines the binary boundary between an application using the framework,
// third-party user module implementations.

#include "clock.hpp"
#include "data.hpp"
#include "metadata.hpp"
#include "lib_signals/signals.hpp"

namespace Modules {

struct IProcessor {
	virtual ~IProcessor() noexcept(false) {}
	virtual void process() = 0;
};

struct IConnectedCap {
	virtual ~IConnectedCap() noexcept(false) {}
	virtual int getNumConnections() const = 0;
	virtual void connect() = 0;
	virtual void disconnect() = 0;
};

struct IInput : public IProcessor, public virtual IConnectedCap, public virtual IMetadataCap {
	virtual ~IInput() noexcept(false) {}
	virtual void push(Data) = 0;

	// TODO: remove this, should only be visible to the module implementations.
	virtual Data pop() = 0;
	virtual bool tryPop(Data &value) = 0;
};

struct IInputCap {
	virtual ~IInputCap() noexcept(false) {}
	virtual int getNumInputs() const = 0;
	virtual IInput* getInput(int i) = 0;
};

struct IOutput : virtual IMetadataCap {
	virtual ~IOutput() noexcept(false) {}
	virtual void emit(Data data) = 0;
	virtual Signals::ISignal<void(Data)>& getSignal() = 0;
};

// FIXME: remove this (see below)
#include <memory>
#include <vector>

struct IOutputCap {
		virtual ~IOutputCap() noexcept(false) {}
		virtual int getNumOutputs() const = 0;
		virtual IOutput* getOutput(int i) = 0;

	protected:
		/*FIXME: we need to have factories to move these back to the implementation - otherwise ports created from the constructor may crash*/
		std::vector<std::unique_ptr<IOutput>> outputs;
		/*const*/ size_t allocatorSize = 0;
};

struct IModule : IProcessor, virtual IInputCap, virtual IOutputCap, virtual IClockCap {
	virtual ~IModule() noexcept(false) {}
	virtual void flush() {}
};

}
