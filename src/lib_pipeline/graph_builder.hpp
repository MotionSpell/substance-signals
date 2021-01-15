#pragma once

#include "pipeline.hpp"
#include "lib_utils/small_map.hpp"

namespace Pipelines {

/*
User-defined function to provide the configuration of the modules.

Signals doesn't provide any serialization mechanism for the configuration of
modules. Therefore the user is required to supply a function which converts a
string from the input JSON configuration to the actual type.

ConfigType is there to erase the variety of configuration types.
*/
typedef int* ConfigType;
typedef std::shared_ptr<ConfigType> ParseModuleConfig(std::string const &ModuleType, SmallMap<std::string /*name*/, std::string /*value*/> const &ModuleParams);

std::unique_ptr<Pipeline> createPipelineFromJSON(const std::string &json, std::function<ParseModuleConfig> parseModuleConfig);

}
