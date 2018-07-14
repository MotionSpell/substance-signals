#pragma once

#include "module.hpp"

namespace Modules {

using Signals::IExecutor;

template<typename Class, typename Function>
Signals::MemberFunctor<Class, void(Class::*)()>
Bind(Function func, Class* objectPtr) {
	return Signals::MemberFunctor<Class, void(Class::*)()>(objectPtr, func);
}

extern Signals::ExecutorSync g_executorSync;
void ConnectOutputToInput(IOutput *prev, IInput *next, IExecutor * const executor = &g_executorSync);
void ConnectModules(IModule *prev, int outputIdx, IModule *next, int inputIdx, IExecutor &executor = g_executorSync);

}
