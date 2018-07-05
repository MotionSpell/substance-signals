#pragma once

// Helpers to make it easier to implement custom modules.
// The application code should not depend on this.

#include "../core/module.hpp"
#include "../core/allocator.hpp"
#include "../core/log.hpp"
#include "../core/error.hpp"
#include "../core/database.hpp" // Data, DataLoose
#include "lib_signals/helper.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/clock.hpp"
#include "lib_utils/tools.hpp" // uptr
#include <memory>
#include <atomic>

namespace Modules {

class MetadataCap : public virtual IMetadataCap {
	public:
		MetadataCap(std::shared_ptr<const IMetadata> metadata = nullptr);
		virtual ~MetadataCap() noexcept(false) {}

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
		ConnectedCap() : connections(0) {}
		virtual ~ConnectedCap() noexcept(false) {}
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
		std::atomic<int> connections;
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
		int getNumInputs() const override {
			return (int)inputs.size();
		}
		IInput* getInput(int i) override {
			return inputs[i].get();
		}

	protected:
		std::vector<std::unique_ptr<IInput>> inputs;
};

static Signals::ExecutorSync<void(Data)> g_executorOutputSync;

template<typename DataType>
class OutputDataDefault : public IOutput, public MetadataCap {
	public:
		OutputDataDefault(size_t allocatorBaseSize, size_t allocatorMaxSize, std::shared_ptr<const IMetadata> metadata = nullptr)
			: MetadataCap(metadata), signal(g_executorOutputSync), allocator(new PacketAllocator(allocatorBaseSize, allocatorMaxSize)) {
		}
		OutputDataDefault(size_t allocatorSize, const IMetadata *metadata = nullptr)
			: OutputDataDefault(allocatorSize, allocatorSize, metadata) {
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

class Module : public IModule, public ErrorCap, public LogCap, public InputCap {
	public:
		Module() = default;
		virtual ~Module() noexcept(false) {}

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
struct ModuleDefault : public ClockCap, public OutputCap, public InstanceType {
	template <typename ...Args>
	ModuleDefault(size_t allocatorSize, std::shared_ptr<IClock> clock, Args&&... args)
		: ClockCap(clock), OutputCap(allocatorSize), InstanceType(std::forward<Args>(args)...) {
	}
};

template <typename InstanceType, typename ...Args>
std::unique_ptr<InstanceType> createModule(size_t allocatorSize, std::shared_ptr<IClock> clock, Args&&... args) {
	return make_unique<ModuleDefault<InstanceType>>(allocatorSize, clock, std::forward<Args>(args)...);
}

template <typename InstanceType, typename ...Args>
std::unique_ptr<InstanceType> create(Args&&... args) {
	return createModule<InstanceType>(ALLOC_NUM_BLOCKS_DEFAULT, nullptr, std::forward<Args>(args)...);
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
		virtual bool work() = 0;
		void process() override;
		bool mustExit();
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
		int getNumInputs() const override {
			if (inputs.size() == 0)
				return 1;
			else if (inputs[inputs.size() - 1]->getNumConnections() == 0)
				return (int)inputs.size();
			else
				return (int)inputs.size() + 1;
		}
		IInput* getInput(int i) override {
			if (i == (int)inputs.size())
				addInput(new Input<DataLoose>(this));
			else if (i > (int)inputs.size())
				throw std::runtime_error(format("Incorrect port number %s for dynamic input.", i));

			return inputs[i].get();
		}
		std::vector<int> getInputs() const {
			std::vector<int> r;
			for (int i = 0; i < getNumInputs() - 1; ++i)
				r.push_back(i);
			return r;
		}

};

template<typename Lambda>
void ConnectOutput(IModule* sender, Lambda f) {
	Connect(sender->getOutput(0)->getSignal(), f);
}

}
