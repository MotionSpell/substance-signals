#pragma once

#include "lib_modules/core/data.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/resolution.hpp"

namespace Modules {

enum PixelFormat {
	UNKNOWN_PF = -1,
	Y8,
	YUV420P,
	YUV420P10LE,
	YUV422P,
	YUV422P10LE,
	YUYV422,
	NV12,
	NV12P010LE, /*10-bit variant of NV12 with 16 bits per component (10 bits of data plus 6 LSB bits zeroed)*/
	RGB24,
	RGBA32,
	SIZE_OF_ENUM_PIXEL_FORMAT
};

class PictureFormat {
	public:
		PictureFormat() : format(UNKNOWN_PF) {
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
			case Y8: return res.width * res.height;
			case YUV420P: return res.width * res.height * 3 / 2;
			case YUV420P10LE: return res.width * divUp(10, 8) * res.height * 3 / 2;
			case YUV422P: return res.width * res.height * 2;
			case YUV422P10LE: return res.width * divUp(10, 8) * res.height * 2;
			case YUYV422: return res.width * res.height * 2;
			case NV12: return res.width * res.height * 3 / 2;
			case NV12P010LE: return res.width * res.height * 3;
			case RGB24: return res.width * res.height * 3;
			case RGBA32: return res.width * res.height * 4;
			default: throw std::runtime_error("Unknown pixel format. Please contact your vendor.");
			}
		}

		bool hasTransparency() const {
			return format == RGBA32;
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

		bool isRecyclable() const override {
			return true;
		}

		PictureFormat getFormat() const {
			return format;
		}
		size_t getSize() const {
			return format.getSize();
		}
		virtual size_t getNumPlanes() const = 0;
		virtual const uint8_t* getPlane(size_t planeIdx) const = 0;
		virtual uint8_t* getPlane(size_t planeIdx) = 0;
		virtual size_t getPitch(size_t planeIdx) const = 0;
		virtual void setInternalResolution(Resolution res) = 0;
		virtual void setVisibleResolution(Resolution res) = 0;

	protected:
		DataPicture(Resolution res, PixelFormat format)
			: DataRaw(PictureFormat::getSize(res, format)), format(res, format), internalFormat(res, format) {
		}

		PictureFormat format;
		PictureFormat internalFormat /*we might need to store the picture within a wider memory space*/;
};

// used by one unit test
class PictureYUV420P : public DataPicture {
	public:
		PictureYUV420P(size_t /*unused*/) : DataPicture(0) {
			internalFormat.format = format.format = YUV420P;
		}
		PictureYUV420P(Resolution res) : DataPicture(res, YUV420P) {
			setInternalResolution(res);
			setVisibleResolution(res);
		}
		size_t getNumPlanes() const override {
			return 3;
		}
		const uint8_t* getPlane(size_t planeIdx) const override {
			return m_planes[planeIdx];
		}
		uint8_t* getPlane(size_t planeIdx) override {
			return m_planes[planeIdx];
		}
		size_t getPitch(size_t planeIdx) const override {
			return m_pitch[planeIdx];
		}
		void setInternalResolution(Resolution res) override {
			internalFormat.res = res;
			resize(internalFormat.getSize());
			auto const numPixels = res.width * res.height;
			m_planes[0] = data().ptr;
			m_planes[1] = data().ptr + numPixels;
			m_planes[2] = data().ptr + numPixels + numPixels / 4;
			m_pitch[0] = res.width;
			m_pitch[1] = res.width / 2;
			m_pitch[2] = res.width / 2;
		}
		void setVisibleResolution(Resolution res) override {
			format.res = res;
		}

	private:
		size_t m_pitch[3];
		uint8_t* m_planes[3];
};

enum VideoCodecType {
	Software,
	Hardware_qsv,
	Hardware_nvenc
};

}

namespace { //FIXME: should be put in .cpp when lib_media/common is made a separate lib

inline Modules::VideoCodecType encoderType(const std::string &opt_encoder_type) {
	if (opt_encoder_type == "software") {
		return Modules::Software;
	} else if (opt_encoder_type == "quicksync") {
		return Modules::Hardware_qsv;
	} else if (opt_encoder_type == "nvenc") {
		return Modules::Hardware_nvenc;
	} else
		throw std::runtime_error("Unknown encoder type. Aborting.");
}
}
