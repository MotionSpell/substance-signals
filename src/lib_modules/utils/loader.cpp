#include <memory>
#include <fstream>
#include "lib_utils/os.hpp"
#include "lib_utils/tools.hpp"
#include "lib_modules/core/module.hpp"
#include "lib_modules/utils/factory.hpp"

using namespace std;

namespace Modules {
static unique_ptr<DynLib> loadPlugin(const char* name) {
	string path = name;

	if(!ifstream(path).is_open())
		path = thisExeDir() + name;

	if(!ifstream(path).is_open())
		throw runtime_error("Module shared object '" + string(name) + "' not found");

	return loadLibrary(path.c_str());
}

shared_ptr<IModule> vLoadModule(const char* name, va_list va) {
	string libName = name + string(".smd");
	auto lib = shared_ptr<DynLib>(loadPlugin(libName.c_str()));
	auto func = (decltype(instantiate)*)lib->getSymbol("instantiate");

	auto deleter = [lib](IModule* mod) {
		delete mod;
	};

	return shared_ptr<IModule>(func(name, va), deleter);
}

shared_ptr<IModule> loadModule(const char* name, ...) {
	va_list va;
	va_start(va, name);
	return vLoadModule(name, va);
}

}
