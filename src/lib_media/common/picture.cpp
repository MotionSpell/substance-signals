#include "picture.hpp"

namespace Modules {
std::shared_ptr<DataPicture> DataPicture::create(OutputPicture *out, const Resolution &res, const Resolution &resInternal, const PixelFormat &format) {
	if (!out) return nullptr;
	std::shared_ptr<DataPicture> r;
	auto const size = PictureFormat::getSize(resInternal, format);
	switch (format) {
	case Y8:          r = safe_cast<DataPicture>(out->getBuffer<PictureY8>(size));          break;
	case YUV420P:     r = safe_cast<DataPicture>(out->getBuffer<PictureYUV420P>(size));     break;
	case YUV420P10LE: r = safe_cast<DataPicture>(out->getBuffer<PictureYUV420P10LE>(size)); break;
	case YUV422P:     r = safe_cast<DataPicture>(out->getBuffer<PictureYUV422P>(size));     break;
	case YUV422P10LE: r = safe_cast<DataPicture>(out->getBuffer<PictureYUV422P10LE>(size));     break;
	case YUYV422:     r = safe_cast<DataPicture>(out->getBuffer<PictureYUYV422>(size));     break;
	case NV12:        r = safe_cast<DataPicture>(out->getBuffer<PictureNV12   >(size));     break;
	case RGB24:       r = safe_cast<DataPicture>(out->getBuffer<PictureRGB24  >(size));     break;
	case RGBA32:      r = safe_cast<DataPicture>(out->getBuffer<PictureRGBA32 >(size));     break;
	default: throw std::runtime_error("Unknown pixel format for DataPicture. Please contact your vendor");
	}
	r->setInternalResolution(resInternal);
	r->setVisibleResolution(res);
	return r;
}

std::shared_ptr<DataPicture> DataPicture::create(OutputPicture *out, const Resolution &res, const PixelFormat &format) {
	return create(out, res, res, format);
}
}
