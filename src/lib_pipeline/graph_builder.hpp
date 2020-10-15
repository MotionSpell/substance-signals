#pragma once

#include "pipeline.hpp"
#include "lib_utils/small_map.hpp"

namespace Pipelines {

typedef void* ParseModuleConfig(std::string, SmallMap<std::string, std::string>&);
std::unique_ptr<Pipeline> createPipelineFromJSON(std::string json, std::function<ParseModuleConfig> parseModuleConfig);

}
