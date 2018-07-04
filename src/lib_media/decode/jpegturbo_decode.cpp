#include "jpegturbo_decode.hpp"
#include "../common/metadata.hpp"
#include "lib_utils/tools.hpp"

extern "C" {
#include <turbojpeg.h>
}

namespace Modules {
namespace Decode {

JPEGTurboDecode::JPEGTurboDecode()
	: jtHandle(tjInitDecompress()) {
	auto input = addInput(new Input<DataBase>(this));
	input->setMetadata(make_shared<MetadataPktVideo>());
	output = addOutput<OutputPicture>(make_shared<MetadataRawVideo>());
}

JPEGTurboDecode::~JPEGTurboDecode() {
	tjDestroy(jtHandle);
}

void JPEGTurboDecode::ensureMetadata(int /*width*/, int /*height*/, int /*pixelFmt*/) {
	if (!output->getMetadata()) {
		auto p = safe_cast<const MetadataRawVideo>(output->getMetadata());
		//TODO: add resolution and pixel format to MetadataRawVideo
		//ctx->width = width;
		//ctx->height = height;
		//ctx->pix_fmt = getAVPF(pixelFmt);
		//output->setMetadata(new MetadataRawVideo(ctx));
	}
}

void JPEGTurboDecode::process(Data data_) {
	auto data = safe_cast<const DataBase>(data_);
	const int pixelFmt = TJPF_RGB;
	int w=0, h=0, jpegSubsamp=0;
	auto buf = (unsigned char*)data->data();
	auto size = (unsigned long)data->size();
	if (tjDecompressHeader2(jtHandle, buf, size, &w, &h, &jpegSubsamp) < 0) {
		log(Warning, "error encountered while decompressing header.");
		return;
	}
	auto out = DataPicture::create(output, Resolution(w, h), RGB24);
	if (tjDecompress2(jtHandle, buf, size, out->data(), w, 0/*pitch*/, h, pixelFmt, TJFLAG_FASTDCT) < 0) {
		log(Warning, "error encountered while decompressing frame.");
		return;
	}
	ensureMetadata(w, h, pixelFmt);
	out->setMediaTime(data->getMediaTime());
	output->emit(out);
}

}
}
