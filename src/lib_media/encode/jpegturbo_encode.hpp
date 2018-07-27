#pragma once

#include "../common/picture.hpp"
#include "lib_modules/core/log.hpp"
#include "lib_modules/utils/helper.hpp"

#define JPEG_DEFAULT_QUALITY 70

namespace Modules {
namespace Encode {

typedef void* tjhandle;

class JPEGTurboEncode : public ModuleS, private LogCap {
	public:
		JPEGTurboEncode(int quality = JPEG_DEFAULT_QUALITY);
		~JPEGTurboEncode();
		void process(Data data) override;

	private:
		OutputDefault* output;
		tjhandle jtHandle;
		int quality;
};

}
}
