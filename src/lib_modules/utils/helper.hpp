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

class MetadataCap {
	public:
		MetadataCap(Metadata metadata = nullptr);

		Metadata getMetadata() const {
			return m_metadata;
		}
		void setMetadata(Metadata metadata);
		bool updateMetadata(Data &data);

	private:
		bool setMetadataInternal(Metadata metadata);

		Metadata m_metadata;
};

static Signals::ExecutorSync g_executorOutputSync;

template<typename DataType>
class OutputDataDefault : public IOutput {
	public:
		OutputDataDefault(size_t allocatorMaxSize, Metadata metadata = nullptr)
			: m_metadataCap(metadata), signal(g_executorOutputSync), allocator(new PacketAllocator(allocatorMaxSize)) {
		}
		virtual ~OutputDataDefault() {
			allocator->unblock();
		}

		void post(Data data) override {
			m_metadataCap.updateMetadata(data);
			signal.emit(data);
		}

		std::shared_ptr<DataType> getBuffer(size_t size) {
			return allocator->alloc<DataType>(size, allocator);
		}

		Signals::ISignal<Data>& getSignal() override {
			return signal;
		}

		void connect(IInput* next) {
			signal.connect([=](Data data) {
				next->push(data);
				next->process();
			});
		}

		void disconnect() override {
			signal.disconnectAll();
		}

		void resetAllocator(size_t allocatorSize) {
			allocator = make_shared<PacketAllocator>(allocatorSize);
		}

		Metadata getMetadata() const override {
			return m_metadataCap.getMetadata();
		}

		void setMetadata(Metadata metadata) override {
			m_metadataCap.setMetadata(metadata);
		}

	private:
		MetadataCap m_metadataCap;
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

class Module : public IModule {
	public:
		int getNumInputs() const override {
			return (int)inputs.size();
		}
		IInput* getInput(int i) override {
			return inputs[i].get();
		}

		int getNumOutputs() const override {
			return (int)outputs.size();
		}
		IOutput* getOutput(int i) override {
			return outputs[i].get();
		}

	protected:
		KInput* addInput(IProcessor* p);

		template <typename InstanceType>
		InstanceType* addOutput() {
			auto p = new InstanceType(allocatorSize);
			outputs.push_back(uptr(p));
			return p;
		}

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
std::unique_ptr<InstanceType> createModuleWithSize(size_t allocatorSize, Args&&... args) {
	return make_unique<ModuleDefault<InstanceType>>(allocatorSize, std::forward<Args>(args)...);
}

template <typename InstanceType, typename ...Args>
std::unique_ptr<InstanceType> createModule(Args&&... args) {
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

template<typename Lambda>
void ConnectOutput(IModule* sender, Lambda f) {
	sender->getOutput(0)->getSignal().connect(f);
}

struct NullHostType : KHost {
	void log(int, char const*) override;
	void activate(bool) override {};
};

static NullHostType NullHost;
}
