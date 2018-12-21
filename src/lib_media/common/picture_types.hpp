#pragma once

namespace Modules {

struct PictureYUV420P : DataPicture {
	PictureYUV420P(Resolution res) : DataPicture(res, PixelFormat::I420) {
		m_planeCount = 3;
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
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
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
		m_planes[0] = data().ptr;
		m_stride[0] = res.width;
		m_planeCount = 1;
	}
};

struct PictureYUV420P10LE : DataPicture {
	PictureYUV420P10LE(Resolution res) : DataPicture(res, PixelFormat::YUV420P10LE) {
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
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
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
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
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
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
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
		m_planes[0] = data().ptr;
		m_stride[0] = res.width * 2;
		m_planeCount = 1;
	}
};

struct PictureNV12 : DataPicture {
	PictureNV12(Resolution res) : DataPicture(res, PixelFormat::NV12) {
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
		auto const numPixels = res.width * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPixels;
		m_stride[0] = m_stride[1] = res.width;
		m_planeCount = 2;
	}
};

struct PictureNV12P010LE : DataPicture {
	PictureNV12P010LE(Resolution res) : DataPicture(res, PixelFormat::NV12P010LE) {
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
		auto const numPixels = res.width * res.height;
		m_planes[0] = data().ptr;
		m_planes[1] = data().ptr + numPixels * 2;
		m_stride[0] = m_stride[1] = res.width * 2;
		m_planeCount = 2;
	}
};

struct PictureRGB24 : DataPicture {
	PictureRGB24(Resolution res) : DataPicture(res, PixelFormat::RGB24) {
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
		m_planes[0] = data().ptr;
		m_stride[0] = res.width * 3;
		m_planeCount = 1;
	}
};

struct PictureRGBA32 : DataPicture {
	PictureRGBA32(Resolution res) : DataPicture(res, PixelFormat::RGBA32) {
		setPlanesAndStrides(res);
		setVisibleResolution(res);
	}
	void setPlanesAndStrides(Resolution res) override {
		m_planes[0] = data().ptr;
		m_stride[0] = res.width * 4;
		m_planeCount = 1;
	}
};

}
