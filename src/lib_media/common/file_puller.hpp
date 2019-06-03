#pragma once

#include <vector>
#include <stdint.h>

namespace Modules {
namespace In {

struct IFilePuller {
	virtual ~IFilePuller() = default;
	virtual std::vector<uint8_t> wget(const char* url) = 0;
};

}
}

#include <vector>

namespace Modules {
namespace In {

inline std::vector<uint8_t> download(Modules::In::IFilePuller* puller, const char* url) {
	return puller->wget(url);
}

}
}

