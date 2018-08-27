#pragma once

#include "lib_modules/utils/helper.hpp"
#include "lib_modules/core/log.hpp"

namespace Modules {
namespace In {

class File : public ActiveModule {
	public:
		File(IModuleHost* host, std::string const& fn);
		~File();
		bool work() override;

	private:
		IModuleHost* const m_host;
		FILE *file;
		OutputDefault *output;
};

}
}
