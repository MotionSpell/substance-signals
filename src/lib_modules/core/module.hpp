#pragma once

#include "clock.hpp"
#include "data.hpp"
#include "error.hpp"
#include "input.hpp"
#include "log.hpp"
#include "output.hpp"
#include <memory>

namespace Modules {

class IModule : public IProcessor, public virtual IInputCap, public virtual IOutputCap, public virtual IClockCap {
	public:
		virtual ~IModule() noexcept(false) {}
		virtual void flush() {}
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

}
