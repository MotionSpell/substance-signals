#pragma once

#include <stdint.h>
#include <string>
#include "../common/utc_start_time.hpp"

struct HlsMuxConfigLibav {
	uint64_t segDurationInMs;
	std::string baseDir;
	std::string baseName;
	std::string options = "";
	IUtcStartTimeQuery* utcStartTime = &g_NullStartTime;
};

