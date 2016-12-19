#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"

namespace Modules {
namespace Transform {

class TeletextToTTML : public ModuleS {
public:
	TeletextToTTML();
	void process(Data data) override;
	void flush() override;

private:
	OutputDataDefault<DataAVPacket> *output;
};

}
}
