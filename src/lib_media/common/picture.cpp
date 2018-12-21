#include "picture.hpp"
#include "picture_types.hpp"

namespace Modules {

std::shared_ptr<DataPicture> DataPicture::create(OutputPicture *out, Resolution res, Resolution resInternal, PixelFormat format) {
	if (!out) return nullptr;
	std::shared_ptr<DataPicture> r;

	switch (format) {
	case PixelFormat::Y8:          r = out->getBuffer<PictureY8>(0);          break;
	case PixelFormat::I420:        r = out->getBuffer<PictureYUV420P>(0);     break;
	case PixelFormat::YUV420P10LE: r = out->getBuffer<PictureYUV420P10LE>(0); break;
	case PixelFormat::YUV422P:     r = out->getBuffer<PictureYUV422P>(0);     break;
	case PixelFormat::YUV422P10LE: r = out->getBuffer<PictureYUV422P10LE>(0); break;
	case PixelFormat::YUYV422:     r = out->getBuffer<PictureYUYV422>(0);     break;
	case PixelFormat::NV12:        r = out->getBuffer<PictureNV12   >(0);     break;
	case PixelFormat::NV12P010LE:  r = out->getBuffer<PictureNV12P010LE>(0);  break;
	case PixelFormat::RGB24:       r = out->getBuffer<PictureRGB24  >(0);     break;
	case PixelFormat::RGBA32:      r = out->getBuffer<PictureRGBA32 >(0);     break;
	default: throw std::runtime_error("Unknown pixel format for DataPicture. Please contact your vendor");
	}

	// 16 bytes of padding, as required by most SIMD processing (e.g swscale)
	r->resize(PictureFormat::getSize(resInternal, format) + 16);
	r->setPlanesAndStrides(resInternal);
	r->setVisibleResolution(res);
	return r;
}

std::shared_ptr<DataPicture> DataPicture::create(OutputPicture *out, Resolution res, PixelFormat format) {
	return create(out, res, res, format);
}
}
