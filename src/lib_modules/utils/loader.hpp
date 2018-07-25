#pragma once
#include <cstdarg>
#include <memory>

namespace Modules {
std::shared_ptr<IModule> loadModule(const char* name, ...);
std::shared_ptr<IModule> vLoadModule(const char* name, va_list);
}

