#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Out {

//Open bar output. Thread-safe by design ©
class Null : public ModuleS {
	public:
		Null();
		void process(Data data) override;
};

}
}
