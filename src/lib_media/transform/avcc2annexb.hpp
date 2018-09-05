#pragma once

#include "lib_modules/modules.hpp"
#include "lib_modules/utils/helper.hpp"

namespace Modules {

class DataAVPacket;

namespace Transform {

struct AVCC2AnnexBConverter : public ModuleS {
		AVCC2AnnexBConverter(IModuleHost* host);
		void process(Data data) override;

	private:
		IModuleHost* const m_host;
		OutputDataDefault<DataAVPacket> *output;
};

}
}
