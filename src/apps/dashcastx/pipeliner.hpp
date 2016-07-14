#pragma once

#include "lib_modules/utils/pipeline.hpp"
#include "options.hpp"

enum FormatFlags {
	NONE = 0,
	MPEG_DASH = 1,
	APPLE_HLS = 1 << 1,
};
inline FormatFlags operator | (FormatFlags a, FormatFlags b) {
	return static_cast<FormatFlags>(static_cast<int>(a) | static_cast<int>(b));
}

void declarePipeline(Pipelines::Pipeline &pipeline, const AppOptions &opt, const FormatFlags formats);
