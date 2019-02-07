#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace In {

class File : public Module {
	public:
		File(KHost* host, std::string const& fn, int blockSize=0);
		~File();
		void process() override;

	private:
		KHost* const m_host;
		FILE *file;
		OutputDefault *output;
		int m_blockSize;
};

}
}
