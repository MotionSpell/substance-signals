#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_utils/clock.hpp"

namespace Modules {

IModule* createSdlVideo(IClock* clock = nullptr);

}

