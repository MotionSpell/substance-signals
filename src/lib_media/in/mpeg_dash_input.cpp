#include "lib_utils/tools.hpp"
#include "mpeg_dash_input.hpp"

#define IOSIZE (64*1024)

namespace Modules {
namespace In {

MPEG_DASH_Input::MPEG_DASH_Input(std::string const &url) {
	//GET MPD FROM HTTP

	//PARSE MPD
	
	//DECLARE OUTPUT PORTS

}

MPEG_DASH_Input::~MPEG_DASH_Input() {
}

void MPEG_DASH_Input::process(Data data) {
	for (;;) {
		if (getNumInputs() && getInput(0)->tryPop(data))
			break;

#if 0
		auto out = output->getBuffer(IOSIZE);
		size_t read = fread(out->data(), 1, IOSIZE, file);
		if (read < IOSIZE) {
			if (read == 0) {
				break;
			}
			out->resize(read);
		}
		output->emit(out);
#endif
	}
}

}
}
