#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Out {

//Open bar output. Thread-safe by design �
struct Null : public ModuleS {
		Null(IModuleHost* host);
		void process(Data data) override;

	private:
		IModuleHost* const m_host;
};

}
}
