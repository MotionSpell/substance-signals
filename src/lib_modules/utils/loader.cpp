#include <memory>
#include <fstream>
#include "lib_utils/os.hpp"
#include "lib_utils/tools.hpp"
#include "lib_modules/core/module.hpp"
#include "lib_modules/utils/factory.hpp"

using namespace std;

namespace Modules {

static string locatePlugin(const char* name) {

	vector<string> candidatePath = {
		name,
		thisExeDir() + name,
	};

	for(auto path : candidatePath) {
		if(ifstream(path).is_open())
			return path;
	}

	return ""; // not found
}

static unique_ptr<DynLib> loadPlugin(const char* name) {

	auto path = locatePlugin(name);

	if(path.empty())
		throw runtime_error("Module shared object '" + string(name) + "' not found");

	return loadLibrary(path.c_str());
}

shared_ptr<IModule> vLoadModule(const char* name, KHost* host, const void* va) {
	string libName = name + string(".smd");

	if(locatePlugin(libName.c_str()) == "") {
		// create plugin from our own static (internal) factory
		return shared_ptr<IModule>(Factory::instantiateModule(name, host, const_cast<void*>(va)));
	} else {
		// create plugin from the shared library's factory
		auto lib = shared_ptr<DynLib>(loadPlugin(libName.c_str()));
		auto func = (decltype(instantiate)*)lib->getSymbol("instantiate");

		auto deleter = [lib](IModule* mod) {
			delete mod;
		};

		return shared_ptr<IModule>(func(name, host, const_cast<void*>(va)), deleter);
	}
}

shared_ptr<IModule> loadModule(const char* name, KHost* host, const void* va) {
	return vLoadModule(name, host, va);
}

}
