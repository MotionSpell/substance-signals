#include "../core/module.hpp" // IModule
#include <cstdarg> // va_list

namespace Modules {
IModule* vInstantiate(const char* name, IModuleHost* host, va_list);

using ModuleCreationFunc = IModule* (IModuleHost*, va_list);
int registerModule(const char* name, ModuleCreationFunc* func);
}

// binary entry-point
#ifdef _MSC_VER
#define EXPORT
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C"
{
	EXPORT Modules::IModule* instantiate(const char* name, Modules::IModuleHost* host, va_list va);
}
