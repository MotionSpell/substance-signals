#pragma once

// Helpers to make it easier to implement custom modules.
// The application code should not depend on this.

#include "../core/module.hpp"
#include "../core/allocator.hpp"
#include "../core/error.hpp"
#include "../core/database.hpp" // Data, Metadata
#include "lib_signals/helper.hpp"
#include "lib_signals/signals.hpp" // Signals::Signal
#include "lib_utils/tools.hpp" // uptr
#include <memory>

namespace Modules {

class MetadataCap : public virtual IMetadataCap {
	public:
		MetadataCap(Metadata metadata = nullptr);
		virtual ~MetadataCap() {}

		Metadata getMetadata() const override {
			return m_metadata;
		}
		void setMetadata(Metadata metadata) override;
		bool updateMetadata(Data &data) override;

	private:
		bool setMetadataInternal(Metadata metadata);

		Metadata m_metadata;
};

static Signals::ExecutorSync g_executorOutputSync;

template<typename DataType>
class OutputDataDefault : public IOutput, public MetadataCap {
	public:
		OutputDataDefault(size_t allocatorMaxSize, Metadata metadata = nullptr)
			: MetadataCap(metadata), signal(g_executorOutputSync), allocator(new PacketAllocator(allocatorMaxSize)) {
		}
		virtual ~OutputDataDefault() {
			allocator->unblock();
		}

		void post(Data data) override {
			updateMetadata(data);
			signal.emit(data);
		}

		template<typename T = DataType>
		std::shared_ptr<T> getBuffer(size_t size) {
			return allocator->alloc<T>(size, allocator);
		}

		Signals::ISignal<Data>& getSignal() override {
			return signal;
		}

		void resetAllocator(size_t allocatorSize) {
			allocator = make_shared<PacketAllocator>(allocatorSize);
		}

	private:
		Signals::Signal<Data> signal;
		std::shared_ptr<PacketAllocator> allocator;
};

typedef OutputDataDefault<DataRaw> OutputDefault;

class OutputCap : public virtual IOutputCap {
	public:
		OutputCap(size_t allocatorSize) {
			this->allocatorSize = allocatorSize;
		}
};

class Module : public IModule, public ErrorCap {
	public:
		KInput* addInput(IProcessor* p);
		int getNumInputs() const override {
			return (int)inputs.size();
		}
		IInput* getInput(int i) override {
			return inputs[i].get();
		}

		template <typename InstanceType, typename ...Args>
		InstanceType* addOutput(Args&&... args) {
			auto p = new InstanceType(allocatorSize, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return p;
		}
		template <typename InstanceType, typename ...Args>
		InstanceType* addOutputDynAlloc(size_t allocatorMaxSize, Args&&... args) {
			auto p = new InstanceType(allocatorMaxSize, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return p;
		}
		int getNumOutputs() const override {
			return (int)outputs.size();
		}
		IOutput* getOutput(int i) override {
			return outputs[i].get();
		}

	protected:
		std::vector<std::unique_ptr<IInput>> inputs;
		std::vector<std::unique_ptr<IOutput>> outputs;
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
		virtual bool work() = 0; // return false to stop processing (e.g eos)
};

template<typename Lambda>
void ConnectOutput(IModule* sender, Lambda f) {
	Connect(sender->getOutput(0)->getSignal(), f);
}

struct NullHostType : KHost {
	void log(int, char const*) override;
};

static NullHostType NullHost;
}
