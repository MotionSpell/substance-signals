#pragma once

#include "module.hpp"

namespace Modules {

void CheckMetadataCompatibility(IOutput *prev, IInput *next);
void ConnectOutputToInput(IOutput *prev, IInput *next);
void ConnectModules(IModule *prev, int outputIdx, IModule *next, int inputIdx);

}
