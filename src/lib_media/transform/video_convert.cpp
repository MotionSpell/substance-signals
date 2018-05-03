#include "lib_utils/tools.hpp"
#include "video_convert.hpp"
#include "lib_ffpp/ffpp.hpp"
#include "../common/libav.hpp"

#define ALIGN_PAD(n, align, pad) ((n/align + 1) * align + pad)

namespace {
inline AVPixelFormat libavPixFmtConvert(const Modules::PixelFormat format) {
	AVPixelFormat pixFmt;
	pixelFormat2libavPixFmt(format, pixFmt);
	return pixFmt;
}
}

namespace Modules {
namespace Transform {

VideoConvert::VideoConvert(const PictureFormat &dstFormat)
	: m_SwContext(nullptr), dstFormat(dstFormat) {
	auto input = addInput(new Input<DataPicture>(this));
	input->setMetadata(shptr(new MetadataRawVideo));
	output = addOutput<OutputPicture>();
}

void VideoConvert::reconfigure(const PictureFormat &format) {
	sws_freeContext(m_SwContext);
	m_SwContext = sws_getContext(format.res.width, format.res.height, libavPixFmtConvert(format.format),
	        dstFormat.res.width, dstFormat.res.height, libavPixFmtConvert(dstFormat.format),
	        SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (!m_SwContext)
		throw error("Impossible to set up video converter.");
	srcFormat = format;
}

VideoConvert::~VideoConvert() {
	sws_freeContext(m_SwContext);
}

void VideoConvert::process(Data data) {
	auto videoData = safe_cast<const DataPicture>(data);
	if (videoData->getFormat() != srcFormat) {
		if (m_SwContext)
			log(Info, "Incompatible input video data. Reconfiguring.");
		reconfigure(videoData->getFormat());
	}

	uint8_t const* srcSlice[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	int srcStride[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	for (size_t i=0; i<videoData->getNumPlanes(); ++i) {
		srcSlice[i] = videoData->getPlane(i);
		srcStride[i] = (int)videoData->getPitch(i);
	}

	std::shared_ptr<DataBase> out;
	uint8_t* pDst[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	int dstStride[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	switch (dstFormat.format) {
	case Y8: case YUV420P: case YUV420P10LE: case YUV422P: case YUV422P10LE: case YUYV422: case NV12: case RGB24: case RGBA32: {
		auto resInternal = Resolution(ALIGN_PAD(dstFormat.res.width, 16, 0), ALIGN_PAD(dstFormat.res.height, 8, 0));
		auto pic = DataPicture::create(output, dstFormat.res, resInternal, dstFormat.format);
		for (size_t i=0; i<pic->getNumPlanes(); ++i) {
			pDst[i] = pic->getPlane(i);
			dstStride[i] = (int)pic->getPitch(i);
		}
		out = pic;
		break;
	}
	default:
		throw error("Destination colorspace not supported.");
	}

	sws_scale(m_SwContext, srcSlice, srcStride, 0, srcFormat.res.height, pDst, dstStride);

	out->setMediaTime(data->getMediaTime());
	output->emit(out);
}

}
}
