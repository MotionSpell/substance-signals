#pragma once

#include "pipeline.hpp"

namespace Pipelines {

std::unique_ptr<Pipeline> createPipelineFromJSON(std::string json);

}
