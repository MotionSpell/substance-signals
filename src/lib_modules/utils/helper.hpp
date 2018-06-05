#pragma once

// Helpers to make it easier to implement custom modules.
// The application code should not depend on this.

#include "../core/module.hpp"
#include "../core/allocator.hpp"
#include "../core/log.hpp"
#include "../core/error.hpp"
#include "lib_signals/utils/helper.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/default_clock.hpp"
#include <memory>
#include <atomic>

namespace Modules {

static bool operator==(const IMetadata &left, const IMetadata &right) {
	return typeid(left) == typeid(right);
}

class MetadataCap : public virtual IMetadataCap {
	public:
		MetadataCap(std::shared_ptr<const IMetadata> metadata = nullptr) : m_metadata(metadata) {}
		virtual ~MetadataCap() noexcept(false) {}

		std::shared_ptr<const IMetadata> getMetadata() const override {
			return m_metadata;
		}
		void setMetadata(std::shared_ptr<const IMetadata> metadata) override {
			if (!setMetadataInternal(metadata))
				throw std::runtime_error("Metadata could not be set.");
		}

		bool updateMetadata(Data &data) override {
			if (!data) {
				return false;
			} else {
				auto const &metadata = data->getMetadata();
				if (!metadata) {
					const_cast<DataBase*>(data.get())->setMetadata(m_metadata);
					return true;
				} else {
					return setMetadataInternal(metadata);
				}
			}
		}

	private:
		bool setMetadataInternal(const std::shared_ptr<const IMetadata> &metadata) {
			if (metadata != m_metadata) {
				if (m_metadata) {
					if (metadata->getStreamType() != m_metadata->getStreamType()) {
						throw std::runtime_error(format("Metadata update: incompatible types %s for data and %s for attached", metadata->getStreamType(), m_metadata->getStreamType()));
					} else if (*m_metadata == *metadata) {
						Log::msg(Debug, "Output: metadata not equal but comparable by value. Updating.");
						m_metadata = metadata;
					} else {
						Log::msg(Info, "Metadata update from data not supported yet: output port and data won't carry the same metadata.");
					}
					return true;
				}
				Log::msg(Debug, "Output: metadata transported by data changed. Updating.");
				m_metadata = metadata;
				return true;
			} else {
				return false;
			}
		}

		std::shared_ptr<const IMetadata> m_metadata;
};

class ConnectedCap : public virtual IConnectedCap {
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

template<typename DataType>
class Input : public IInput, public ConnectedCap, public MetadataCap {
	public:
		Input(IProcessor * const processor) : processor(processor) {}

		void push(Data data) override {
			queue.push(data);
		}

		Data pop() override {
			return queue.pop();
		}

		bool tryPop(Data& data) override {
			return queue.tryPop(data);
		}

		void clear() {
			return queue.clear();
		}

		void process() override {
			processor->process();
		}

	private:
		IProcessor * const processor;
		Queue<Data> queue;
};

class InputCap : public virtual IInputCap {
	public:
		virtual ~InputCap() noexcept(false) {}
		IInput* addInput(IInput* p) { //Takes ownership
			inputs.push_back(uptr(p));
			return p;
		}
		size_t getNumInputs() const override {
			return inputs.size();
		}
		IInput* getInput(size_t i) override {
			return inputs[i].get();
		}

	protected:
		std::vector<std::unique_ptr<IInput>> inputs;
};

typedef Signals::Signal<void(Data), Signals::ResultQueue<NotVoid<void>>> SignalAsync;
typedef Signals::Signal<void(Data), Signals::ResultVector<NotVoid<void>>> SignalSync;
static Signals::ExecutorSync<void(Data)> g_executorOutputSync;
typedef SignalSync SignalDefaultSync;

template<typename DataType>
class OutputDataDefault : public IOutput, public MetadataCap, public ClockCap {
	public:
		typedef PacketAllocator<DataType> Allocator;
		typedef SignalDefaultSync Signal;

		OutputDataDefault(size_t allocatorBaseSize, size_t allocatorMaxSize, std::shared_ptr<IClock> clock, std::shared_ptr<const IMetadata> metadata = nullptr)
			: MetadataCap(metadata), ClockCap(clock), signal(g_executorOutputSync), allocator(new Allocator(allocatorBaseSize, allocatorMaxSize)) {
		}
		OutputDataDefault(size_t allocatorSize, std::shared_ptr<IClock> clock, const IMetadata *metadata = nullptr)
			: OutputDataDefault(allocatorSize, allocatorSize, clock, metadata) {
		}
		virtual ~OutputDataDefault() noexcept(false) {
			allocator->unblock();
		}

		void emit(Data data) override {
			updateMetadata(data);
			auto numReceivers = signal.emit(data);
			if (numReceivers == 0)
				Log::msg(Debug, "emit(): Output had no receiver");
		}

		template<typename T = typename Allocator::MyType>
		std::shared_ptr<T> getBuffer(size_t size) {
			auto buffer = allocator->template getBuffer<T>(size, allocator);
			if (clock) buffer->setCreationTime(fractionToClock(clock->now()));
			return buffer;
		}

		Signals::ISignal<void(Data)>& getSignal() override {
			return signal;
		}

	private:
		Signal signal;
		std::shared_ptr<Allocator> allocator;
};

typedef OutputDataDefault<DataRaw> OutputDefault;

class OutputCap : public virtual IOutputCap {
	public:
		OutputCap(size_t allocatorSize) {
			this->allocatorSize = allocatorSize;
		}

		size_t getNumOutputs() const override {
			return outputs.size();
		}
		IOutput* getOutput(size_t i) override {
			return outputs[i].get();
		}
};

class Module : public IModule, public ErrorCap, public LogCap, public InputCap {
	public:
		Module() = default;
		virtual ~Module() noexcept(false) {}

		template <typename InstanceType, typename ...Args>
		InstanceType* addOutput(Args&&... args) {
			auto p = new InstanceType(allocatorSize, allocatorSize, clock, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return p;
		}
		template <typename InstanceType, typename ...Args>
		InstanceType* addOutputDynAlloc(size_t allocatorMaxSize, Args&&... args) {
			auto p = new InstanceType(allocatorSize, allocatorMaxSize, clock, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return p;
		}
};

/* this default factory creates output ports with the default output - create another one for other uses such as low latency */
template <class InstanceType>
struct ModuleDefault : public ClockCap, public OutputCap, public InstanceType {
	template <typename ...Args>
	ModuleDefault(size_t allocatorSize, const std::shared_ptr<IClock> clock, Args&&... args)
		: ClockCap(clock), OutputCap(allocatorSize), InstanceType(std::forward<Args>(args)...) {
	}
};

template <typename InstanceType, typename ...Args>
std::unique_ptr<InstanceType> createModule(size_t allocatorSize, const std::shared_ptr<IClock> clock, Args&&... args) {
	return make_unique<ModuleDefault<InstanceType>>(allocatorSize, clock, std::forward<Args>(args)...);
}

template <typename InstanceType, typename ...Args>
std::unique_ptr<InstanceType> create(Args&&... args) {
	return createModule<InstanceType>(ALLOC_NUM_BLOCKS_DEFAULT, g_DefaultClock, std::forward<Args>(args)...);
}

//single input specialized module
class ModuleS : public Module {
	public:
		ModuleS() = default;
		virtual ~ModuleS() noexcept(false) {}
		virtual void process(Data data) = 0;
		void process() override {
			process(getInput(0)->pop());
		}
};

//dynamic input number specialized module
//note: ports added automatically will carry the DataLoose type which doesn't
//      allow to perform all safety checks ; consider adding ports manually if
//      you can
class ModuleDynI : public Module {
	public:
		ModuleDynI() = default;
		virtual ~ModuleDynI() noexcept(false) {}

		IInput* addInput(IInput *p) { //takes ownership
			bool isDyn = false;
			std::unique_ptr<IInput> pEx;
			if (inputs.size() && dynamic_cast<DataLoose*>(inputs.back().get())) {
				isDyn = true;
				pEx = std::move(inputs.back());
				inputs.pop_back();
			}
			inputs.push_back(uptr(p));
			if (isDyn)
				inputs.push_back(std::move(pEx));
			return p;
		}
		size_t getNumInputs() const override {
			if (inputs.size() == 0)
				return 1;
			else if (inputs[inputs.size() - 1]->getNumConnections() == 0)
				return inputs.size();
			else
				return inputs.size() + 1;
		}
		IInput* getInput(size_t i) override {
			if (i == inputs.size())
				addInput(new Input<DataLoose>(this));
			else if (i > inputs.size())
				throw std::runtime_error(format("Incorrect port number %s for dynamic input.", i));

			return inputs[i].get();
		}
		std::vector<size_t> getInputs() const {
			std::vector<size_t> r;
			for (size_t i = 0; i < getNumInputs() - 1; ++i)
				r.push_back(i);
			return r;
		}

};

template<typename Lambda>
void ConnectOutput(IModule* sender, Lambda f) {
	Connect(sender->getOutput(0)->getSignal(), f);
}

}
