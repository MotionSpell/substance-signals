#include <cstdarg> // va_list

namespace Modules {

class IModule;
class IModuleHost;

namespace Factory {
using CreationFunc = IModule* (IModuleHost*, va_list);
IModule* instantiateModule(const char* name, IModuleHost* host, va_list);
int registerModule(const char* name, CreationFunc* func);
}

}

///////////////////////////////////////////////////////////////////////////////
// prototype for binary entry-point (implemented by user modules)
///////////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C"
{
	EXPORT Modules::IModule* instantiate(const char* name, Modules::IModuleHost* host, va_list va);
}
