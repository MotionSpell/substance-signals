#include <cstdarg> // va_list

namespace Modules {

struct IModule;
struct KHost;

namespace Factory {
using CreationFunc = IModule* (KHost*, va_list);
IModule* instantiateModule(const char* name, KHost* host, va_list);
int registerModule(const char* name, CreationFunc* func);
}

}

///////////////////////////////////////////////////////////////////////////////
// prototype for binary entry-point to the UM shared library
///////////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C"
{
	EXPORT Modules::IModule* instantiate(const char* name, Modules::KHost* host, va_list va);
}
