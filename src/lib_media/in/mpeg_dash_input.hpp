#pragma once

#include "lib_modules/core/module.hpp"

namespace Modules {
namespace In {

class MPEG_DASH_Input : public ModuleS {
	public:
		MPEG_DASH_Input(std::string const &url);
		~MPEG_DASH_Input();
		void process(Data data) override;

	private:
};

}
}
