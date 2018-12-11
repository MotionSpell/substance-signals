#pragma once

#include <vector>

namespace Modules {
namespace In {

struct IFilePuller {
	virtual ~IFilePuller() = default;
	virtual std::vector<uint8_t> get(std::string url) = 0;
};

}
}

