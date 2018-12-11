#pragma once

#include <vector>
#include <stdint.h>

namespace Modules {
namespace In {

struct IFilePuller {
	virtual ~IFilePuller() = default;
	virtual std::vector<uint8_t> get(const char* url) = 0;
};

}
}

