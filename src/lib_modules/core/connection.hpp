#pragma once

#include "module.hpp"
#include "lib_signals/helper.hpp" // Signals::MemberFunctor
#include "lib_signals/executor.hpp" // ExecutorSync

namespace Modules {

using Signals::IExecutor;

template<typename Class, typename Function>
Signals::MemberFunctor<Class, void(Class::*)()>
Bind(Function func, Class* objectPtr) {
	return Signals::MemberFunctor<Class, void(Class::*)()>(objectPtr, func);
}

extern Signals::ExecutorSync g_executorSync;
void ConnectOutputToInput(IOutput *prev, IInput *next);
void ConnectModules(IModule *prev, int outputIdx, IModule *next, int inputIdx);

}
