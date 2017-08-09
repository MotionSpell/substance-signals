#pragma once

#include "allocator.hpp"
#include "clock.hpp"
#include "data.hpp"
#include "metadata.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/tools.hpp"
#include <lib_signals/signals.hpp>
#include <memory>
#include <vector>

namespace Modules {

class IModule;

typedef Signals::Signal<void(Data), Signals::ResultQueue<NotVoid<void>>> SignalAsync;
typedef Signals::Signal<void(Data), Signals::ResultVector<NotVoid<void>>> SignalSync;
static Signals::ExecutorSync<void(Data)> g_executorOutputSync;
typedef SignalSync SignalDefaultSync;

class IOutput : public virtual IMetadataCap {
public:
	virtual ~IOutput() noexcept(false) {}
	virtual size_t emit(Data data) = 0;
	virtual Signals::ISignal<void(Data)>& getSignal() = 0;
};

template<typename Allocator, typename Signal>
class OutputT : public IOutput, public MetadataCap, public ClockCap {
public:
	typedef Allocator AllocatorType;

	OutputT(size_t allocatorBaseSize, size_t allocatorMaxSize, std::shared_ptr<IClock> clock, std::shared_ptr<IMetadata> metadata = nullptr)
		: MetadataCap(metadata), ClockCap(clock), signal(g_executorOutputSync), allocator(new Allocator(allocatorBaseSize, allocatorMaxSize)) {
	}
	OutputT(size_t allocatorSize, std::shared_ptr<IClock> clock, const IMetadata *metadata = nullptr)
		: OutputT(allocatorSize, allocatorSize, clock, metadata) {
	}
	virtual ~OutputT() noexcept(false) {
		allocator->unblock();
	}

	size_t emit(Data data) override {
		updateMetadata(data);
		size_t numReceivers = signal.emit(data);
		if (numReceivers == 0)
			Log::msg(Debug, "emit(): Output had no receiver");
		return numReceivers;
	}

	template<typename T = typename Allocator::MyType>
	std::shared_ptr<T> getBuffer(size_t size) {
		auto buffer = allocator->template getBuffer<T>(size, allocator);
		if (clock) buffer->setClockTime(clock->now(), 1000);
		return buffer;
	}

	Signals::ISignal<void(Data)>& getSignal() override {
		return signal;
	}

private:
	Signal signal;
	std::shared_ptr<Allocator> allocator;
};

template<typename DataType> using OutputDataDefault = OutputT<PacketAllocator<DataType>, SignalDefaultSync>;
typedef OutputDataDefault<DataRaw> OutputDefault;

class IOutputCap {
public:
	virtual ~IOutputCap() noexcept(false) {}
	virtual size_t getNumOutputs() const = 0;
	virtual IOutput* getOutput(size_t i) const = 0;

protected:
	/*FIXME: we need to have factories to move these back to the implementation - otherwise pins created from the constructor may crash*/
	std::vector<std::unique_ptr<IOutput>> outputs;
	/*const*/ size_t allocatorSize;
};

class OutputCap : public virtual IOutputCap {
public:
	OutputCap(size_t allocatorSize) {
		this->allocatorSize = allocatorSize;
	}

	size_t getNumOutputs() const override {
		return outputs.size();
	}
	IOutput* getOutput(size_t i) const override {
		return outputs[i].get();
	}
};

}
