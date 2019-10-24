#pragma once

#include "lib_modules/core/buffer.hpp" // SpanC
#include <functional>
#include <stdint.h>

namespace Modules {
namespace In {

struct IFilePuller {
	virtual ~IFilePuller() = default;
	virtual void wget(const char* url, std::function<void(SpanC)> callback) = 0;
};

}
}

#include <vector>

namespace Modules {
namespace In {

inline std::vector<uint8_t> download(Modules::In::IFilePuller* puller, const char* url) {
	std::vector<uint8_t> r;
	auto onBuffer = [&](SpanC buf) {
		for(auto c : buf)
			r.push_back(c);
	};
	puller->wget(url, onBuffer);
	return r;
}

}
}

