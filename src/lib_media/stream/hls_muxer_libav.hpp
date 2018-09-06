#pragma once

#include <stdint.h>
#include <string>

struct HlsMuxConfigLibav {
	uint64_t segDurationInMs;
	std::string baseDir;
	std::string baseName;
	std::string options = "";
};

