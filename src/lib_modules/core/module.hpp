#pragma once

// This is the binary boundary between:
// - binary applications (plugin host for binary 'signals' plugins).
// - binary third-party 'signals' modules ("UM").
//
// This is the only header file that third-parties are expected to include.
// Do not pass or return STL objects or concrete classes here.

#include "data.hpp"
#include "metadata.hpp"
#include "lib_signals/signals.hpp"

namespace Modules {

// This is how the framework sees custom module implementations.
//
// As these interfaces are implemented by third-parties,
// they should be kept small and leave almost no room for errors.
// (stuff like connection lists and pin lists should be kept in the framework).
//

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
}

// FIXME: remove this (see below)
#include <memory>
#include <vector>

namespace Modules {

struct IOutputCap {
		virtual ~IOutputCap() noexcept(false) {}
		virtual int getNumOutputs() const = 0;
		virtual IOutput* getOutput(int i) = 0;

	protected:
		/*FIXME: we need to have factories to move these back to the implementation - otherwise ports created from the constructor may crash*/
		std::vector<std::unique_ptr<IOutput>> outputs;
		/*const*/ size_t allocatorSize = 0;
};

struct IModule : IProcessor, virtual IInputCap, virtual IOutputCap {
	virtual ~IModule() noexcept(false) {}
	virtual void flush() {}
};

}
