#include "lib_modules/utils/helper.hpp" // ModuleS
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_media/common/picture.hpp" // PictureFormat
#include "lib_utils/tools.hpp"
#include "../common/ffpp.hpp"
#include "../common/libav.hpp"

extern "C" {
#include <libswscale/swscale.h>
}

static size_t ALIGN_PAD(size_t n, size_t align) {
	return ((n/align + 1) * align);
}

using namespace Modules;

namespace {

class VideoConvert : public ModuleS {
	public:
		VideoConvert(KHost* host, const PictureFormat &dstFormat);
		~VideoConvert();
		void process(Data data) override;

	private:
		void reconfigure(const PictureFormat &format);

		KHost* const m_host;
		SwsContext *m_SwContext;
		PictureFormat srcFormat, dstFormat;
		OutputPicture* output;
};

VideoConvert::VideoConvert(KHost* host, const PictureFormat &dstFormat)
	: m_host(host),
	  m_SwContext(nullptr), dstFormat(dstFormat) {
	auto input = addInput(this);
	input->setMetadata(make_shared<MetadataRawVideo>());
	output = addOutput<OutputPicture>();
}

void VideoConvert::reconfigure(const PictureFormat &format) {
	sws_freeContext(m_SwContext);
	m_SwContext = sws_getContext(format.res.width, format.res.height, pixelFormat2libavPixFmt(format.format),
	        dstFormat.res.width, dstFormat.res.height, pixelFormat2libavPixFmt(dstFormat.format),
	        SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (!m_SwContext)
		throw error("Impossible to set up video converter.");
	m_host->log(Info, ::format("Converter configured to: %sx%s:%s -> %sx%s:%s",
	        format.res.width, format.res.height, (int)format.format,
	        dstFormat.res.width, dstFormat.res.height, (int)dstFormat.format
	    ).c_str());
	srcFormat = format;
}

VideoConvert::~VideoConvert() {
	sws_freeContext(m_SwContext);
}

void VideoConvert::process(Data data) {
	auto videoData = safe_cast<const DataPicture>(data);
	if (videoData->getFormat() != srcFormat) {
		reconfigure(videoData->getFormat());
	}

	uint8_t const* srcSlice[8] {};
	int srcStride[8] {};
	for (size_t i=0; i<videoData->getNumPlanes(); ++i) {
		srcSlice[i] = videoData->getPlane(i);
		srcStride[i] = (int)videoData->getPitch(i);
	}

	std::shared_ptr<DataBase> out;
	uint8_t* pDst[8] {};
	int dstStride[8] {};
	switch (dstFormat.format) {
	case PixelFormat::Y8:
	case PixelFormat::I420:
	case PixelFormat::YUV420P10LE:
	case PixelFormat::YUV422P:
	case PixelFormat::YUV422P10LE:
	case PixelFormat::YUYV422:
	case PixelFormat::NV12:
	case PixelFormat::NV12P010LE:
	case PixelFormat::RGB24:
	case PixelFormat::RGBA32: {
		auto resInternal = Resolution(ALIGN_PAD(dstFormat.res.width, 16), ALIGN_PAD(dstFormat.res.height, 8));
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

Modules::IModule* createObject(KHost* host, va_list va) {
	auto fmt = va_arg(va, PictureFormat*);
	enforce(host, "VideoConvert: host can't be NULL");
	return Modules::create<VideoConvert>(host, *fmt).release();
}

auto const registered = Factory::registerModule("VideoConvert", &createObject);
}
