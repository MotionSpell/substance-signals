#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Utils {

class Recorder : public ModuleS {
	public:
		Recorder(IModuleHost* host);
		void process(Data data) override;
		void flush() override;

		Data pop();

	private:
		IModuleHost* const m_host;
		Queue<Data> record;
};

}
}
