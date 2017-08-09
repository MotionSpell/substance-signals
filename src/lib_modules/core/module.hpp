#pragma once

#include "clock.hpp"
#include "data.hpp"
#include "error.hpp"
#include "input.hpp"
#include "log.hpp"
#include "output.hpp"
#include "../utils/helper.hpp"
#include <memory>

namespace Modules {

class IModule : public IProcessor, public virtual IInputCap, public virtual IOutputCap, public virtual IClockCap {
public:
	virtual ~IModule() noexcept(false) {}
	virtual void flush() {}
};

class Module : public IModule, public ErrorCap, public LogCap, public InputCap, public ClockCap {
public:
	Module(const std::shared_ptr<IClock> clock = g_DefaultClock) {}
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
//note: pins added automatically will carry the DataLoose type which doesn't
//      allow to perform all safety checks ; consider adding pins manually if
//      you can
class ModuleDynI : public Module {
	public:
		ModuleDynI() = default;
		virtual ~ModuleDynI() noexcept(false) {}

		IInput* addInput(IInput *p) override { //takes ownership
			bool isDyn = false;
			std::unique_ptr<IInput> pEx(nullptr);
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
				throw std::runtime_error(format("Incorrect pin number %s for dynamic input.", i));

			return inputs[i].get();
		}
};

}
