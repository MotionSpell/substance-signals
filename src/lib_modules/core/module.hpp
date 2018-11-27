#pragma once

// This is the binary boundary between:
// - binary applications (plugin host for binary 'signals' plugins).
// - binary third-party 'signals' modules ("UM").
//
// This is the only header file that third-parties are expected to include.
// Do not pass or return STL objects or concrete classes here.

#include "buffer.hpp"
#include "metadata.hpp"

// FIXME: remove this, only needed for metadata
#include <memory>

namespace Modules {

// This is how user modules see the outside world.
// 'K' interfaces are called by the user module implementations.
struct KInput {
	virtual ~KInput() = default;
	virtual Data pop() = 0;
	virtual bool tryPop(Data &value) = 0;
	virtual void setMetadata(std::shared_ptr<const IMetadata> metadata) = 0;
};

struct KOutput {
	virtual ~KOutput() = default;
	virtual void emit(Data data) = 0;
};

struct KHost {
	virtual ~KHost() = default;
	virtual void log(int level, char const* msg) = 0;
};

struct NullHostType : KHost {
	void log(int, char const*) override;
};

static NullHostType NullHost;
}

// This is how the framework sees custom module implementations.
//
// As these interfaces are implemented by third-parties,
// they should be kept small and leave almost no room for errors.
// (stuff like connection lists and pin lists should be kept in the framework).
//

namespace Signals {
template<typename T> struct ISignal;
}

namespace Modules {

struct IProcessor {
	virtual ~IProcessor() = default;
	virtual void process() = 0;
};

struct IInput : IProcessor, KInput {
	virtual ~IInput() = default;

	virtual int isConnected() const = 0;
	virtual void connect() = 0;
	virtual void disconnect() = 0;

	virtual void push(Data) = 0;

	virtual std::shared_ptr<const IMetadata> getMetadata() const = 0;
	virtual bool updateMetadata(Data&) = 0;
};

struct IOutput : virtual IMetadataCap, KOutput {
	virtual ~IOutput() = default;
	virtual Signals::ISignal<Data>& getSignal() = 0;
};

struct IOutputCap {
		virtual ~IOutputCap() = default;

	protected:
		/*const*/ size_t allocatorSize = 0;
};

struct IModule : IProcessor,  virtual IOutputCap {
	virtual ~IModule() = default;

	virtual int getNumInputs() const = 0;
	virtual IInput* getInput(int i) = 0;

	virtual int getNumOutputs() const = 0;
	virtual IOutput* getOutput(int i) = 0;

	virtual void flush() {}
};

}
