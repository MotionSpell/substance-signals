#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace In {

class File : public ActiveModule, private LogCap {
	public:
		File(std::string const& fn);
		~File();
		bool work() override;

	private:
		FILE *file;
		OutputDefault *output;
};

}
}
