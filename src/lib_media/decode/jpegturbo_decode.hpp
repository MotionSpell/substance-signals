#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/picture.hpp"

namespace Modules {
namespace Decode {

typedef void* tjhandle;

class JPEGTurboDecode : public ModuleS {
	public:
		JPEGTurboDecode();
		~JPEGTurboDecode();
		void process(Data data) override;

	private:
		OutputPicture* output;
		void ensureMetadata(int width, int height, int pixelFmt);
		tjhandle jtHandle;
};

}
}
