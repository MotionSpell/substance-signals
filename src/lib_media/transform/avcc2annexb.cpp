#include "lib_modules/modules.hpp"
#include "lib_modules/utils/helper.hpp"
#include "../common/ffpp.hpp"
#include "../common/attributes.hpp"

extern "C" {
#include <libavformat/avformat.h> // av_packet_copy_props
}

using namespace Modules;

namespace {

struct ByteReader {
	SpanC data;
	size_t pos = 0;

	size_t available() const {
		return data.len - pos;
	}

	uint8_t u8() {
		return data[pos++];
	}

	uint32_t u32() {
		uint32_t r = 0;
		for(int i=0; i < 4; ++i) {
			r <<= 8;
			r += u8();
		}
		return r;
	}

	void read(Span out) {
		memcpy(out.ptr, data.ptr+pos, out.len);
		pos+=out.len;
	}
};

struct AVCC2AnnexBConverter : public ModuleS {
		AVCC2AnnexBConverter(KHost* host)
			: m_host(host) {
			output = addOutput<OutputDefault>();
		}

		void processOne(Data in) override {
			if(isDeclaration(in))
				return;

			auto out = output->allocData<DataRaw>(in->data().len);

			out->copyAttributes(*in);
			out->setMediaTime(in->getMediaTime());

			auto bs = ByteReader { in->data() };
			while ( auto availableBytes = bs.available() ) {
				if (availableBytes < 4) {
					m_host->log(Error, format("Need to read 4 byte start-code, only %s available. Exit current conversion.", availableBytes).c_str());
					break;
				}
				auto const size = bs.u32();
				if (size + 4 > availableBytes) {
					m_host->log(Error, format("Too much data read: %s (available: %s - 4) (total %s). Exit current conversion.", size, availableBytes, in->data().len).c_str());
					break;
				}
				// write start code
				auto bytes = out->getBuffer()->data().ptr + bs.pos - 4;
				*bytes++ = 0x00;
				*bytes++ = 0x00;
				*bytes++ = 0x00;
				*bytes++ = 0x01;
				bs.read({out->getBuffer()->data().ptr + bs.pos, size});
			}

			out->setMetadata(in->getMetadata());
			out->setMediaTime(in->getMediaTime());
			output->post(out);
		}
	private:
		KHost* const m_host;
		OutputDefault* output;
};

IModule* createObject(KHost* host, void* va) {
	(void)va;
	enforce(host, "AVCC2AnnexBConverter: host can't be NULL");
	return new ModuleDefault<AVCC2AnnexBConverter>(1, host);
}

auto const registered = Factory::registerModule("AVCC2AnnexBConverter", &createObject);
}
