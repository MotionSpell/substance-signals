#pragma once

#include "module.hpp"
#include "lib_signals/helper.hpp" // Signals::MemberFunctor

namespace Modules {

template<typename Class, typename Function>
Signals::MemberFunctor<Class, void(Class::*)()>
Bind(Function func, Class* objectPtr) {
	return Signals::MemberFunctor<Class, void(Class::*)()>(objectPtr, func);
}

void CheckMetadataCompatibility(IOutput *prev, IInput *next);
void ConnectOutputToInput(IOutput *prev, IInput *next);
void ConnectModules(IModule *prev, int outputIdx, IModule *next, int inputIdx);

}
