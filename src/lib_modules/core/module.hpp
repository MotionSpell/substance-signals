#pragma once

#include "clock.hpp"
#include "data.hpp"
#include "error.hpp"
#include "log.hpp"
#include "output.hpp"
#include <memory>

namespace Modules {

struct IProcessor {
	virtual ~IProcessor() noexcept(false) {}
	virtual void process() = 0;
};

class ConnectedCap {
	public:
		ConnectedCap() : connections(0) {}
		virtual ~ConnectedCap() noexcept(false) {}
		virtual size_t getNumConnections() const {
			return connections;
		}
		virtual void connect() {
			connections++;
		}
		virtual void disconnect() {
			connections--;
		}

	private:
		std::atomic_size_t connections;
};

struct IInput : public IProcessor, public ConnectedCap, public MetadataCap, public Queue<Data> {
	virtual ~IInput() noexcept(false) {}
};

struct IInputCap {
	virtual ~IInputCap() noexcept(false) {}
	virtual IInput* addInput(IInput* p) = 0;
	virtual size_t getNumInputs() const = 0;
	virtual IInput* getInput(size_t i) = 0;
};

class IModule : public IProcessor, public virtual IInputCap, public virtual IOutputCap, public virtual IClockCap {
	public:
		virtual ~IModule() noexcept(false) {}
		virtual void flush() {}
};

}
