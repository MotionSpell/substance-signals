#include "../core/module.hpp" // IModule
#include <cstdarg> // va_list

namespace Modules {
IModule* instantiate(const char* name, ...);
IModule* vInstantiate(const char* name, va_list);

using ModuleCreationFunc = IModule* (va_list);
int registerModule(const char* name, ModuleCreationFunc* func);
}

#define EXPORT __attribute__((visibility("default")))

extern "C"
{
	// binary entry-point
	EXPORT Modules::IModule* instantiate(const char* name, va_list va);
}
