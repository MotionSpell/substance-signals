#pragma once

#include "../common/picture.hpp"
#include "lib_modules/utils/helper.hpp"

#define JPEG_DEFAULT_QUALITY 70

namespace Modules {
namespace Encode {

typedef void* tjhandle;

class JPEGTurboEncode : public ModuleS {
	public:
		JPEGTurboEncode(IModuleHost* host, int quality = JPEG_DEFAULT_QUALITY);
		~JPEGTurboEncode();
		void process(Data data) override;

	private:
		IModuleHost* const m_host;
		OutputDefault* output;
		tjhandle jtHandle;
		int quality;
};

}
}
