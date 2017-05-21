#pragma once

#include "lib_pipeline/pipeline.hpp"
#include "options.hpp"

std::unique_ptr<Pipelines::Pipeline> buildPipeline(const IConfig &config);
