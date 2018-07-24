#include "../core/module.hpp" // IModule
#include <cstdarg> // va_list

namespace Modules {
IModule* instanciate(const char* name, ...);

using ModuleCreationFunc = IModule* (va_list);
int registerModule(const char* name, ModuleCreationFunc* func);
}

