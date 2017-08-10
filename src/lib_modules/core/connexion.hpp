#include "module.hpp"

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
size_t ConnectModules(Module *prev, size_t outputIdx, Module *next, size_t inputIdx, IProcessExecutor &executor = defaultExecutor);

}
