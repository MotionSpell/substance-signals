#include "../core/module.hpp" // IModule
#include <cstdarg> // va_list

namespace Modules {
IModule* vInstantiate(const char* name, va_list);

using ModuleCreationFunc = IModule* (va_list);
int registerModule(const char* name, ModuleCreationFunc* func);
}

