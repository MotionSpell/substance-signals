#pragma once

// Helpers to make it easier to implement custom modules.
// The application code should not depend on this.

#include "../core/module.hpp"
#include "../core/allocator.hpp"
#include "../core/error.hpp"
#include "../core/database.hpp" // Data, DataLoose
#include "lib_signals/helper.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/tools.hpp" // uptr
#include <memory>

namespace Modules {

class MetadataCap : public virtual IMetadataCap {
	public:
		MetadataCap(std::shared_ptr<const IMetadata> metadata = nullptr);
		virtual ~MetadataCap() {}

		std::shared_ptr<const IMetadata> getMetadata() const override {
			return m_metadata;
		}
		void setMetadata(std::shared_ptr<const IMetadata> metadata) override;
		bool updateMetadata(Data &data) override;

	private:
		bool setMetadataInternal(const std::shared_ptr<const IMetadata> &metadata);

		std::shared_ptr<const IMetadata> m_metadata;
};

class ConnectedCap : public virtual IConnectedCap {
	public:
		virtual int getNumConnections() const {
			return connections;
		}
		virtual void connect() {
			connections++;
		}
		virtual void disconnect() {
			connections--;
		}

	private:
		int connections = 0;
};

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
		virtual ~InputCap() {}
		IInput* createInput(IProcessor* p) {
			inputs.push_back(std::make_unique<Input>(p));
			return inputs.back().get();
		}
		int getNumInputs() const override {
			return (int)inputs.size();
		}
		IInput* getInput(int i) override {
			return inputs[i].get();
		}

	protected:
		std::vector<std::unique_ptr<IInput>> inputs;
};

static Signals::ExecutorSync g_executorOutputSync;

template<typename DataType>
class OutputDataDefault : public IOutput, public MetadataCap {
	public:
		OutputDataDefault(size_t allocatorBaseSize, size_t allocatorMaxSize, std::shared_ptr<const IMetadata> metadata = nullptr)
			: MetadataCap(metadata), signal(g_executorOutputSync), allocator(new PacketAllocator(allocatorBaseSize, allocatorMaxSize)) {
		}
		OutputDataDefault(size_t allocatorSize, const IMetadata *metadata = nullptr)
			: OutputDataDefault(allocatorSize, allocatorSize, metadata) {
		}
		virtual ~OutputDataDefault() {
			allocator->unblock();
		}

		void emit(Data data) override {
			updateMetadata(data);
			signal.emit(data);
		}

		template<typename T = DataType>
		std::shared_ptr<T> getBuffer(size_t size) {
			return allocator->template getBuffer<T>(size, allocator);
		}

		Signals::ISignal<void(Data)>& getSignal() override {
			return signal;
		}

		void resetAllocator(size_t allocatorSize) {
			allocator = make_shared<PacketAllocator>(1, allocatorSize);
		}

	private:
		Signals::Signal<void(Data)> signal;
		std::shared_ptr<PacketAllocator> allocator;
};

typedef OutputDataDefault<DataRaw> OutputDefault;

class OutputCap : public virtual IOutputCap {
	public:
		OutputCap(size_t allocatorSize) {
			this->allocatorSize = allocatorSize;
		}

		int getNumOutputs() const override {
			return (int)outputs.size();
		}
		IOutput* getOutput(int i) override {
			return outputs[i].get();
		}
};

class Module : public IModule, public ErrorCap, public InputCap {
	public:
		Module() = default;
		virtual ~Module() {}

		template <typename InstanceType, typename ...Args>
		InstanceType* addOutput(Args&&... args) {
			auto p = new InstanceType(allocatorSize, allocatorSize, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return p;
		}
		template <typename InstanceType, typename ...Args>
		InstanceType* addOutputDynAlloc(size_t allocatorMaxSize, Args&&... args) {
			auto p = new InstanceType(allocatorSize, allocatorMaxSize, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return p;
		}
};

/* this default factory creates output ports with the default output - create another one for other uses such as low latency */
template <class InstanceType>
struct ModuleDefault : public OutputCap, public InstanceType {
	template <typename ...Args>
	ModuleDefault(size_t allocatorSize, Args&&... args)
		: OutputCap(allocatorSize), InstanceType(std::forward<Args>(args)...) {
	}
};

template <typename InstanceType, typename ...Args>
std::unique_ptr<InstanceType> createModule(size_t allocatorSize, Args&&... args) {
	return make_unique<ModuleDefault<InstanceType>>(allocatorSize, std::forward<Args>(args)...);
}

template <typename InstanceType, typename ...Args>
std::unique_ptr<InstanceType> create(Args&&... args) {
	return make_unique<ModuleDefault<InstanceType>>(ALLOC_NUM_BLOCKS_DEFAULT, std::forward<Args>(args)...);
}

//single input specialized module
class ModuleS : public Module {
	public:
		virtual void process(Data data) = 0;
		void process() override {
			process(getInput(0)->pop());
		}
};

//module with no input (=source module)
class ActiveModule : public Module {
	public:
		void process() override;
		bool mustExit = false;
	protected:
		virtual bool work() = 0;
};

template<typename Lambda>
void ConnectOutput(IModule* sender, Lambda f) {
	Connect(sender->getOutput(0)->getSignal(), f);
}

}
