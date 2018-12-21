#pragma once

#include "pixel_format.hpp"
#include "lib_modules/core/buffer.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/resolution.hpp"
#include "lib_utils/fraction.hpp" // divUp

namespace Modules {

class PictureFormat {
	public:
		PictureFormat() : format(PixelFormat::UNKNOWN) {
		}
		PictureFormat(Resolution res, PixelFormat format)
			: res(res), format(format) {
		}
		bool operator==(PictureFormat const& other) const {
			return res == other.res && format == other.format;
		}
		bool operator!=(PictureFormat const& other) const {
			return !(*this == other);
		}

		size_t getSize() const {
			return getSize(res, format);
		}
		static size_t getSize(Resolution res, PixelFormat format) {
			switch (format) {
			case PixelFormat::Y8: return res.width * res.height;
			case PixelFormat::I420: return res.width * res.height * 3 / 2;
			case PixelFormat::YUV420P10LE: return res.width * divUp(10, 8) * res.height * 3 / 2;
			case PixelFormat::YUV422P: return res.width * res.height * 2;
			case PixelFormat::YUV422P10LE: return res.width * divUp(10, 8) * res.height * 2;
			case PixelFormat::YUYV422: return res.width * res.height * 2;
			case PixelFormat::NV12: return res.width * res.height * 3 / 2;
			case PixelFormat::NV12P010LE: return res.width * res.height * 3;
			case PixelFormat::RGB24: return res.width * res.height * 3;
			case PixelFormat::RGBA32: return res.width * res.height * 4;
			default: throw std::runtime_error("Unknown pixel format. Please contact your vendor.");
			}
		}

		bool hasTransparency() const {
			return format == PixelFormat::RGBA32;
		}

		Resolution res;
		PixelFormat format;
};

class DataPicture;
typedef OutputDataDefault<DataPicture> OutputPicture;

//TODO: we should probably separate planar vs non-planar data, avoid resize on the data, etc.
class DataPicture : public DataRaw {
	public:
		DataPicture(size_t /*unused*/) : DataRaw(0) {}
		static std::shared_ptr<DataPicture> create(OutputPicture *out, Resolution res, PixelFormat format);
		static std::shared_ptr<DataPicture> create(OutputPicture *out, Resolution res, Resolution resInternal, PixelFormat format);

		PictureFormat getFormat() const {
			return format;
		}
		size_t getSize() const {
			return format.getSize();
		}
		uint8_t* getPlane(int planeIdx) {
			return m_planes[planeIdx];
		}
		const uint8_t* getPlane(int planeIdx) const {
			return m_planes[planeIdx];
		}
		size_t getStride(int planeIdx) const {
			return m_stride[planeIdx];
		}
		int getNumPlanes() const {
			return m_planeCount;
		}

		virtual void setInternalResolution(Resolution res) = 0;
		virtual void setVisibleResolution(Resolution res) = 0;

	protected:
		DataPicture(Resolution res, PixelFormat format)
			: DataRaw(PictureFormat::getSize(res, format)), format(res, format)  {
		}

		PictureFormat format;

		int m_planeCount = 0;
		size_t m_stride[4];
		uint8_t* m_planes[4];
};
}

