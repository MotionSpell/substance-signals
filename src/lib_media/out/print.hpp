#pragma once

#include "lib_modules/utils/helper.hpp"
#include <ostream>

namespace Modules {
namespace Out {

class Print : public ModuleS {
	public:
		Print(std::ostream &os);
		void process(Data data) override;

	private:
		std::ostream &os;
};

}
}
