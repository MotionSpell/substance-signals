#pragma once
#include "lib_modules/core/module.hpp"

#include <cstdarg>
#include <memory>

namespace Modules {
std::shared_ptr<IModule> loadModule(const char* name, KHost* host, ...);
std::shared_ptr<IModule> vLoadModule(const char* name, KHost* host, va_list);
}

