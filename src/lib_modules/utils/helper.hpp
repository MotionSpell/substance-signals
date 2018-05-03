#pragma once

#include "../core/module.hpp"
#include "lib_signals/utils/helper.hpp"
#include "lib_utils/tools.hpp"
#include <memory>

namespace Modules {

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

}
