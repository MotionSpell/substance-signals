#pragma once

#include "lib_modules/core/data.hpp"
#include "lib_modules/core/output.hpp"
#include "lib_utils/resolution.hpp"
#include <cmath>

namespace Modules {

#undef PixelFormat //there are collisions with FFmpeg here
enum PixelFormat {
	UNKNOWN_PF = -1,
	YUV420P,
	YUV420P10LE,
	YUV422P,
	YUYV422,
	NV12,
	RGB24,
	RGBA32
};

class PictureFormat {
public:
	PictureFormat() : format(UNKNOWN_PF) {
	}
	PictureFormat(const Resolution &res, const PixelFormat &format)
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
	static size_t getSize(const Resolution &res, const PixelFormat &format) {
		switch (format) {
		case YUV420P: return res.width * res.height * 3 / 2;
		case YUV420P10LE: return res.width * divUp(10, 8) * res.height * 3 / 2;
		case YUV422P: return res.width * res.height * 2;
		case YUYV422: return res.width * res.height * 2;
		case NV12: return res.width * res.height * 3 / 2;
		case RGB24: return res.width * res.height * 3;
		case RGBA32: return res.width * res.height * 4;
		default: throw std::runtime_error("Unknown pixel format. Please contact your vendor.");
		}
	}

	Resolution res;
	PixelFormat format;
};

class DataPicture;
typedef OutputDataDefault<DataPicture> OutputPicture;

//TODO: we should probably separate planar vs non-planar data, avoid resize on the data, etc.
class DataPicture : public DataRaw {
public:
	DataPicture(size_t unused) : DataRaw(0) {}
	static std::shared_ptr<DataPicture> create(OutputPicture *out, const Resolution &res, const PixelFormat &format);
	static std::shared_ptr<DataPicture> create(OutputPicture *out, const Resolution &res, const Resolution &resInternal, const PixelFormat &format);

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
	virtual void setInternalResolution(const Resolution &res) = 0;
	virtual void setVisibleResolution(const Resolution &res) = 0;

protected:
	DataPicture(const Resolution &res, const PixelFormat &format)
		: DataRaw(PictureFormat::getSize(res, format)),
			format(res, format), internalFormat(res, format) {
	}

	PictureFormat format;
	PictureFormat internalFormat /*we might need to store the picture within a wider memory space*/;
};

class PictureYUV420P : public DataPicture {
public:
	PictureYUV420P(size_t unused) : DataPicture(0) {
		internalFormat.format = format.format = YUV420P;
	}
	PictureYUV420P(const Resolution &res)
		: DataPicture(res, YUV420P) {
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
	void setInternalResolution(const Resolution &res) override {
		internalFormat.res = res;
		resize(internalFormat.getSize());
		auto const numPixels = res.width * res.height;
		m_planes[0] = data();
		m_planes[1] = data() + numPixels;
		m_planes[2] = data() + numPixels + numPixels / 4;
		m_pitch[0] = res.width;
		m_pitch[1] = res.width / 2;
		m_pitch[2] = res.width / 2;
	}
	void setVisibleResolution(const Resolution &res) override {
		format.res = res;
	}

private:
	size_t m_pitch[3];
	uint8_t* m_planes[3];
};

class PictureYUV420P10LE : public DataPicture {
public:
	PictureYUV420P10LE(size_t unused) : DataPicture(0) {
		internalFormat.format = format.format = YUV420P10LE;
	}
	PictureYUV420P10LE(const Resolution &res)
		: DataPicture(res, YUV420P10LE) {
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
	void setInternalResolution(const Resolution &res) override {
		internalFormat.res = res;
		resize(internalFormat.getSize());
		auto const numPlaneBytes = res.width * divUp(10, 8) * res.height;
		m_planes[0] = data();
		m_planes[1] = data() + numPlaneBytes;
		m_planes[2] = data() + numPlaneBytes + numPlaneBytes / 4;
		m_pitch[0] = res.width * divUp(10, 8);
		m_pitch[1] = res.width * divUp(10, 8) / 2;
		m_pitch[2] = res.width * divUp(10, 8) / 2;
	}
	void setVisibleResolution(const Resolution &res) override {
		format.res = res;
	}

private:
	size_t m_pitch[3];
	uint8_t* m_planes[3];
};

class PictureYUV422P : public DataPicture {
public:
	PictureYUV422P(size_t unused) : DataPicture(0) {
		internalFormat.format = format.format = YUV422P;
	}
	PictureYUV422P(const Resolution &res)
		: DataPicture(res, YUV422P) {
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
	void setInternalResolution(const Resolution &res) override {
		internalFormat.res = res;
		resize(internalFormat.getSize());
		auto const numPixels = res.width * res.height;
		m_planes[0] = data();
		m_planes[1] = data() + numPixels;
		m_planes[2] = data() + numPixels + numPixels / 2;
		m_pitch[0] = res.width;
		m_pitch[1] = res.width / 2;
		m_pitch[2] = res.width / 2;
	}
	void setVisibleResolution(const Resolution &res) override {
		format.res = res;
	}

private:
	size_t m_pitch[3];
	uint8_t* m_planes[3];
};

class PictureYUYV422 : public DataPicture {
public:
	PictureYUYV422(size_t unused) : DataPicture(0) {
		internalFormat.format = format.format = YUYV422;
	}
	PictureYUYV422(const Resolution &res)
		: DataPicture(res, YUYV422) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	size_t getNumPlanes() const override {
		return 1;
	}
	const uint8_t* getPlane(size_t planeIdx) const override {
		return data();
	}
	uint8_t* getPlane(size_t planeIdx) override {
		return data();
	}
	size_t getPitch(size_t planeIdx) const override {
		return format.res.width * 2;
	}
	void setInternalResolution(const Resolution &res) override {
		internalFormat.res = res;
		resize(internalFormat.getSize());
	}
	void setVisibleResolution(const Resolution &res) override {
		format.res = res;
	}
};

class PictureNV12 : public DataPicture {
public:
	PictureNV12(size_t unused) : DataPicture(0) {
		internalFormat.format = format.format = NV12;
	}
	PictureNV12(const Resolution &res)
		: DataPicture(res, NV12) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	size_t getNumPlanes() const override {
		return 2;
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
	void setInternalResolution(const Resolution &res) override {
		internalFormat.res = res;
		resize(internalFormat.getSize());
		auto const numPixels = res.width * res.height;
		m_planes[0] = data();
		m_planes[1] = data() + numPixels;
		m_pitch[0] = m_pitch[1] = res.width;
	}
	void setVisibleResolution(const Resolution &res) override {
		format.res = res;
	}

private:
	size_t m_pitch[2];
	uint8_t* m_planes[2];
};

class PictureRGB24 : public DataPicture {
public:
	PictureRGB24(size_t unused) : DataPicture(0) {
		internalFormat.format = format.format = RGB24;
	}
	PictureRGB24(const Resolution &res)
		: DataPicture(res, RGB24) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	size_t getNumPlanes() const override {
		return 1;
	}
	const uint8_t* getPlane(size_t planeIdx) const override {
		return data();
	}
	uint8_t* getPlane(size_t planeIdx) override {
		return data();
	}
	size_t getPitch(size_t planeIdx) const override {
		return format.res.width * 3;
	}
	void setInternalResolution(const Resolution &res) override {
		internalFormat.res = res;
		resize(internalFormat.getSize());

	}
	void setVisibleResolution(const Resolution &res) override {
		format.res = res;
	}
};

class PictureRGBA32 : public DataPicture {
public:
	PictureRGBA32(size_t unused) : DataPicture(0) {
		internalFormat.format = format.format = RGBA32;
	}
	PictureRGBA32(const Resolution &res)
		: DataPicture(res, RGBA32) {
		setInternalResolution(res);
		setVisibleResolution(res);
	}
	size_t getNumPlanes() const override {
		return 1;
	}
	const uint8_t* getPlane(size_t planeIdx) const override {
		return data();
	}
	uint8_t* getPlane(size_t planeIdx) override {
		return data();
	}
	size_t getPitch(size_t planeIdx) const override {
		return format.res.width * 4;
	}
	void setInternalResolution(const Resolution &res) override {
		internalFormat.res = res;
		resize(internalFormat.getSize());
	}
	void setVisibleResolution(const Resolution &res) override {
		format.res = res;
	}
};

static const Resolution VIDEO_RESOLUTION(320, 180);
static const int VIDEO_FPS = 24;

}

namespace { //FIXME: should be put in .cpp when lib_media/common is made a separate lib
void fps2NumDen(const double fps, int &num, int &den) {
	const double tolerance = 0.001;
	if (fabs(fps - (int)fps) < tolerance) { //integer frame rates
		num = (int)fps;
		den = 1;
	} else if (fabs((fps*1001.0) / 1000.0 - (int)(fps + 1)) < tolerance) { //ATSC frame rates
		num = (int)(fps + 1) * 1000;
		den = 1001;
	} else if (fabs(fps * 2 - (int)(fps * 2)) < tolerance) { //rational frame rates; den = 2
		num = (int)(fps * 2);
		den = 2;
	} else if (fabs(fps * 4 - (int)(fps * 4)) < tolerance) { //rational frame rates; den = 4
		num = (int)(fps * 4);
		den = 4;
	} else {
		num = (int)fps;
		den = 1;
		Log::msg(Warning, "Frame rate '%s' was not recognized. Truncating to '%s'.", fps, num);
	}
}

enum VideoCodecType {
	Software,
	Hardware_qsv,
	Hardware_nvenc
};

VideoCodecType encoderType(const std::string &opt_encoder_type) {
	if (opt_encoder_type == "software") {
		return Software;
	} else if (opt_encoder_type == "quicksync") {
		return Hardware_qsv;
	} else if (opt_encoder_type == "nvenc") {
		return Hardware_nvenc;
	} else
		throw std::runtime_error("Unknown encoder type. Aborting.");
}
}