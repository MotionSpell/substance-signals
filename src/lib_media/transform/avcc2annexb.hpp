#pragma once

#include "lib_modules/modules.hpp"
#include "lib_modules/core/log.hpp"
#include "lib_modules/utils/helper.hpp"

namespace Modules {

class DataAVPacket;

namespace Transform {

struct AVCC2AnnexBConverter : public ModuleS, private LogCap {
		AVCC2AnnexBConverter();
		void process(Data data) override;

	private:
		OutputDataDefault<DataAVPacket> *output;
};

}
}
