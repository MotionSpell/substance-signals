#pragma once

#include "lib_modules/core/module.hpp"

namespace Modules {
namespace In {

struct IHttpSource {
	virtual std::string get(std::string url) = 0;
};

class MPEG_DASH_Input : public Module {
	public:
		MPEG_DASH_Input(IHttpSource* httpSource, std::string const &url);
		void process() override;
};

}
}
