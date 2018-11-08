#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace In {

class File : public ActiveModule {
	public:
		File(KHost* host, std::string const& fn);
		~File();
		bool work() override;

	private:
		KHost* const m_host;
		FILE *file;
		OutputDefault *output;
};

}
}
