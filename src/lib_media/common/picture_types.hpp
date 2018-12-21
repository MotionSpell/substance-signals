#pragma once

namespace Modules {

struct PictureYUV420P : DataPicture {
	PictureYUV420P(Resolution res) : DataPicture(res, PixelFormat::I420) {
		m_planeCount = 3;
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		auto const numPixels = res.width * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPixels;
		m_planes[2] = data().ptr + numPixels + numPixels / 4;
		m_stride[0] = res.width;
		m_stride[1] = res.width / 2;
		m_stride[2] = res.width / 2;
	}
};

struct PictureY8 : DataPicture {
	PictureY8(Resolution res) : DataPicture(res, PixelFormat::Y8) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		m_planes[0] = data().ptr;
		m_stride[0] = res.width;
		m_planeCount = 1;
	}
};

struct PictureYUV420P10LE : DataPicture {
	PictureYUV420P10LE(Resolution res) : DataPicture(res, PixelFormat::YUV420P10LE) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		auto const numPlaneBytes = res.width * divUp(10, 8) * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPlaneBytes;
		m_planes[2] = data().ptr + numPlaneBytes + numPlaneBytes / 4;
		m_stride[0] = res.width * divUp(10, 8);
		m_stride[1] = res.width * divUp(10, 8) / 2;
		m_stride[2] = res.width * divUp(10, 8) / 2;
		m_planeCount = 3;
	}
};

struct PictureYUV422P : DataPicture {
	PictureYUV422P(Resolution res) : DataPicture(res, PixelFormat::YUV422P) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		auto const numPixels = res.width * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPixels;
		m_planes[2] = data().ptr + numPixels + numPixels / 2;
		m_stride[0] = res.width;
		m_stride[1] = res.width / 2;
		m_stride[2] = res.width / 2;
		m_planeCount = 3;
	}
};

struct PictureYUV422P10LE : DataPicture {
	PictureYUV422P10LE(Resolution res) : DataPicture(res, PixelFormat::YUV422P10LE) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		auto const numPixels = res.width * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPixels;
		m_planes[2] = data().ptr + numPixels + numPixels / 2;
		m_stride[0] = res.width * divUp(10, 8);
		m_stride[1] = res.width * divUp(10, 8) / 2;
		m_stride[2] = res.width * divUp(10, 8) / 2;
		m_planeCount = 3;
	}
};

struct PictureYUYV422 : DataPicture {
	PictureYUYV422(Resolution res) : DataPicture(res, PixelFormat::YUYV422) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		m_planes[0] = data().ptr;
		m_stride[0] = res.width * 2;
		m_planeCount = 1;
	}
};

struct PictureNV12 : DataPicture {
	PictureNV12(Resolution res) : DataPicture(res, PixelFormat::NV12) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		auto const numPixels = res.width * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPixels;
		m_stride[0] = m_stride[1] = res.width;
		m_planeCount = 2;
	}
};

struct PictureNV12P010LE : DataPicture {
	PictureNV12P010LE(const Resolution &res) : DataPicture(res, PixelFormat::NV12P010LE) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		auto const numPixels = res.width * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPixels * 2;
		m_stride[0] = m_stride[1] = res.width * 2;
		m_planeCount = 2;
	}
};

struct PictureRGB24 : DataPicture {
	PictureRGB24(Resolution res) : DataPicture(res, PixelFormat::RGB24) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		// 16 bytes of padding, as required by most SIMD processing (e.g swscale)
		resize(PictureFormat::getSize(res, format.format) + 16);
		m_planes[0] = data().ptr;
		m_stride[0] = res.width * 3;
		m_planeCount = 1;
	}
};

struct PictureRGBA32 : DataPicture {
	PictureRGBA32(Resolution res) : DataPicture(res, PixelFormat::RGBA32) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	void setInternalResolution(Resolution res) override {
		resize(PictureFormat::getSize(res, format.format));
		m_planes[0] = data().ptr;
		m_stride[0] = res.width * 4;
		m_planeCount = 1;
	}
};

}
