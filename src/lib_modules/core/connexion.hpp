#include "input.hpp"
#include "output.hpp"

namespace Modules {

typedef Signals::IExecutor<void()> IProcessExecutor;
extern Signals::ExecutorSync<void()> g_executorSync;
#define defaultExecutor g_executorSync

template<typename Class>
Signals::MemberFunctor<void, Class, void(Class::*)()>
MEMBER_FUNCTOR_PROCESS(Class* objectPtr) {
	return Signals::MemberFunctor<void, Class, void(Class::*)()>(objectPtr, &IProcessor::process);
}

size_t ConnectOutputToInput(IOutput *prev, IInput *next, IProcessExecutor * const executor = &defaultExecutor);

template<typename ModuleType1, typename ModuleType2>
size_t ConnectModules(ModuleType1 *prev, size_t outputIdx, ModuleType2 *next, size_t inputIdx, IProcessExecutor &executor = defaultExecutor) {
	auto output = prev->getOutput(outputIdx);
	return ConnectOutputToInput(output, next->getInput(inputIdx), &executor);
}

}
