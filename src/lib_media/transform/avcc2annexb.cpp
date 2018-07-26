#include "avcc2annexb.hpp"
#include "lib_gpacpp/gpacpp.hpp"
#include "lib_media/common/libav.hpp"
#include "lib_ffpp/ffpp.hpp"

namespace Modules {
namespace Transform {

AVCC2AnnexBConverter::AVCC2AnnexBConverter() {
	auto input = addInput(new Input<DataAVPacket>(this));
	output = addOutput<OutputDataDefault<DataAVPacket>>(input->getMetadata());
}

void AVCC2AnnexBConverter::process(Data in) {
	auto out = output->getBuffer(in->size());
	av_packet_copy_props(out->getPacket(), safe_cast<const DataAVPacket>(in)->getPacket());

	auto bs = gf_bs_new((const char*)in->data(), in->size(), GF_BITSTREAM_READ);
	u64 availableBytes, pos;
	while ( (availableBytes = gf_bs_available(bs)) ) {
		if (availableBytes < 4) {
			log(Error, "Need to read 4 byte start-code, only %s available. Exit current conversion.", availableBytes);
			break;
		}
		pos = gf_bs_get_position(bs);
		auto const size = gf_bs_read_u32(bs);
		if (size + 4 > availableBytes) {
			log(Error, "Too much data read: %s (available: %s - 4) (total %s). Exit current conversion.", size, availableBytes, in->size());
			break;
		}
		// write start code
		auto bytes = out->data() + pos;
		*bytes++ = 0x00;
		*bytes++ = 0x00;
		*bytes++ = 0x00;
		*bytes++ = 0x01;
		gf_bs_read_data(bs, (char*)out->data() + pos + 4, size);
	}
	gf_bs_del(bs);

	out->setMetadata(in->getMetadata());
	out->setMediaTime(in->getMediaTime());
	output->emit(out);
}

}
}
