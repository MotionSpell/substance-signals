#pragma once

// Helpers to make it easier to implement custom modules.
// The application code should not depend on this.

#include "../core/module.hpp"
#include "lib_signals/utils/helper.hpp"
#include "lib_utils/tools.hpp"
#include <memory>

namespace Modules {

template<typename DataType, typename ModuleType = IProcessor>
class Input : public IInput {
	public:
		Input(ModuleType * const module) : module(module) {}

		void process() override {
			module->process();
		}

	private:
		ModuleType * const module;
};

class InputCap : public virtual IInputCap {
	public:
		virtual ~InputCap() noexcept(false) {}
		IInput* addInput(IInput* p) override { //Takes ownership
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

class Module : public IModule, public ErrorCap, public LogCap, public InputCap {
	public:
		Module() = default;
		virtual ~Module() noexcept(false) {}

		template <typename InstanceType, typename ...Args>
		InstanceType* addOutput(Args&&... args) {
			auto p = new InstanceType(allocatorSize, allocatorSize, clock, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return safe_cast<InstanceType>(p);
		}
		template <typename InstanceType, typename ...Args>
		InstanceType* addOutputDynAlloc(size_t allocatorMaxSize, Args&&... args) {
			auto p = new InstanceType(allocatorSize, allocatorMaxSize, clock, std::forward<Args>(args)...);
			outputs.push_back(uptr(p));
			return safe_cast<InstanceType>(p);
		}

	private:
		Module(Module const&) = delete;
		Module const& operator=(Module const&) = delete;
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
	return uptr(new ModuleDefault<InstanceType>(allocatorSize, clock, std::forward<Args>(args)...));
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

		IInput* addInput(IInput *p) override { //takes ownership
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
};

}
