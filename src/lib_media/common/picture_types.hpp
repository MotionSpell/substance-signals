#pragma once

namespace Modules {

class PictureYUV420P : public DataPicture {
	public:
		PictureYUV420P(size_t /*unused*/) : DataPicture(0) {
			m_planeCount = 3;
			format.format = PixelFormat::I420;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureY8 : public DataPicture {
	public:
		PictureY8(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::Y8;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureYUV420P10LE : public DataPicture {
	public:
		PictureYUV420P10LE(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::YUV420P10LE;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureYUV422P : public DataPicture {
	public:
		PictureYUV422P(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::YUV422P;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureYUV422P10LE : public DataPicture {
	public:
		PictureYUV422P10LE(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::YUV422P10LE;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureYUYV422 : public DataPicture {
	public:
		PictureYUYV422(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::YUYV422;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureNV12 : public DataPicture {
	public:
		PictureNV12(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::NV12;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureNV12P010LE : public DataPicture {
	public:
		PictureNV12P010LE(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::NV12P010LE;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureRGB24 : public DataPicture {
	public:
		PictureRGB24(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::RGB24;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

class PictureRGBA32 : public DataPicture {
	public:
		PictureRGBA32(size_t /*unused*/) : DataPicture(0) {
			format.format = PixelFormat::RGBA32;
			m_planeCount = 1;
		}
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
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}
};

}
