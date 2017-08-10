#pragma once

#include "../core/module.hpp"
#include "lib_signals/utils/helper.hpp"
#include <memory>

namespace Modules {

typedef Signals::IExecutor<void()> IProcessExecutor;
static Signals::ExecutorSync<void()> g_executorSync;
#define defaultExecutor g_executorSync

/* this default factory creates output pins with the default output - create another one for other uses such as low latency */
template <class InstanceType>
struct ModuleDefault : public ClockCap, public OutputCap, public InstanceType {
	template <typename ...Args>
	ModuleDefault(size_t allocatorSize, const std::shared_ptr<IClock> clock, Args&&... args)
	: ClockCap(clock), OutputCap(allocatorSize), InstanceType(std::forward<Args>(args)...) {
	}
};

template <typename InstanceType, typename ...Args>
InstanceType* createModule(size_t allocatorSize, const std::shared_ptr<IClock> clock, Args&&... args) {
	return new ModuleDefault<InstanceType>(allocatorSize, clock, std::forward<Args>(args)...);
}

template <typename InstanceType, typename ...Args>
InstanceType* create(Args&&... args) {
	return createModule<InstanceType>(ALLOC_NUM_BLOCKS_DEFAULT, g_DefaultClock, std::forward<Args>(args)...);
}

template<typename Class>
Signals::MemberFunctor<void, Class, void(Class::*)()>
MEMBER_FUNCTOR_PROCESS(Class* objectPtr) {
	return Signals::MemberFunctor<void, Class, void(Class::*)()>(objectPtr, &IProcessor::process);
}

template<typename IInput>
size_t ConnectOutputToInput(IOutput *prev, IInput *next, IProcessExecutor * const executor = &defaultExecutor) {
	auto prevMetadata = safe_cast<const IMetadataCap>(prev)->getMetadata();
	auto nextMetadata = next->getMetadata();
	if (prevMetadata && nextMetadata) {
		if (prevMetadata->getStreamType() != next->getMetadata()->getStreamType())
			throw std::runtime_error("Module connection: incompatible types");
		Log::msg(Info, "--------- Connect: metadata OK");
	} else {
		if (prevMetadata && !nextMetadata) {
			Log::msg(Info, "--------- Connect: metadata is not the same as next");
		} else if (!prevMetadata && nextMetadata) {
			safe_cast<IMetadataCap>(prev)->setMetadata(nextMetadata);
			Log::msg(Info, "--------- Connect: metadata propagate to previous (backward)");
		} else {
			Log::msg(Debug, "--------- Connect: no metadata");
		}
	}

	next->connect();
	return prev->getSignal().connect([=](Data data) {
		next->push(data);
		(*executor)(MEMBER_FUNCTOR_PROCESS(next));
	});
}

template<typename ModuleType>
size_t ConnectOutputToInput(IOutput *prev, std::unique_ptr<ModuleType>& next, IProcessExecutor& executor = defaultExecutor) {
	return ConnectOutputToInput(prev, next->getInput(0), &executor);
}

template<typename ModuleType1, typename ModuleType2>
size_t ConnectModules(ModuleType1 *prev, size_t outputIdx, ModuleType2 *next, size_t inputIdx, IProcessExecutor& executor = defaultExecutor) {
	auto output = prev->getOutput(outputIdx);
	return ConnectOutputToInput(output, next->getInput(inputIdx), &executor);
}

}
