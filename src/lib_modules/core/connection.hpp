#include "module.hpp"

namespace Modules {

typedef Signals::IExecutor IProcessExecutor;

template<typename Class, typename Function>
Signals::MemberFunctor<Class, void(Class::*)()>
Bind(Function func, Class* objectPtr) {
	return Signals::MemberFunctor<Class, void(Class::*)()>(objectPtr, func);
}

extern Signals::ExecutorSync g_executorSync;
void ConnectOutputToInput(IOutput *prev, IInput *next, IProcessExecutor * const executor = &g_executorSync);
void ConnectModules(IModule *prev, int outputIdx, IModule *next, int inputIdx, IProcessExecutor &executor = g_executorSync);

}
