#pragma once

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
};

}
