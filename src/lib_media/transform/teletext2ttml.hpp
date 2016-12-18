#pragma once

#include "lib_modules/core/module.hpp"

namespace Modules {
namespace Transform {

class TeletextToTTML : public ModuleS {
public:
	TeletextToTTML();
	void process(Data data) override;
};

}
}
