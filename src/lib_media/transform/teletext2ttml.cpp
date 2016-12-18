#include "teletext2ttml.hpp"
#include "../common/libav.hpp"

namespace Modules {
namespace Transform {

TeletextToTTML::TeletextToTTML() {
	auto input = addInput(new Input<DataAVPacket>(this));
	//input->setMetadata(new MetadataRawAudio);
	addOutput<OutputDataDefault<DataAVPacket>>();
}

void TeletextToTTML::process(Data data) {
	//1. nothing => DONE
	//2. build graph => DONE
	//3. sparse stream : send regularly empty samples
	//4. same ttml
	//5. gpac mux
	//6. real converter
}

}
}
