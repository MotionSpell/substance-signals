#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace In {

struct IFilePuller {
	virtual std::string get(std::string url) = 0;
};

class MPEG_DASH_Input : public Module {
	public:
		MPEG_DASH_Input(IFilePuller* filePuller, std::string const &url);
		void process() override;
};

}
}
