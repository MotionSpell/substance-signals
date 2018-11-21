#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/picture.hpp"

using namespace Tests;
using namespace Modules;
using namespace std;

unittest("video converter: pass-through") {
	auto const res = Resolution(16, 32);
	auto const format = PictureFormat(res, PixelFormat::I420);
	int numFrames = 0;

	auto onFrame = [&](Data data) {
		auto pic = safe_cast<const DataPicture>(data);
		ASSERT(pic->getFormat() == format);
		numFrames++;
	};

	{
		auto convert = loadModule("VideoConvert", &NullHost, &format);
		ConnectOutput(convert.get(), onFrame);

		auto pic = make_shared<PictureYUV420P>(res);
		convert->getInput(0)->push(pic);
		convert->process();
	}

	ASSERT_EQUALS(1, numFrames);
}

unittest("video converter: different sizes") {
	auto const srcRes = Resolution(16, 32);
	auto const dstRes = Resolution(24, 8);
	auto const format = PictureFormat(dstRes, PixelFormat::I420);
	int numFrames = 0;

	auto onFrame = [&](Data data) {
		auto pic = safe_cast<const DataPicture>(data);
		ASSERT(pic->getFormat() == format);
		numFrames++;
	};

	{
		auto convert = loadModule("VideoConvert", &NullHost, &format);
		ConnectOutput(convert.get(), onFrame);

		auto pic = make_shared<PictureYUV420P>(srcRes);
		convert->getInput(0)->push(pic);
		convert->process();
	}

	ASSERT_EQUALS(1, numFrames);
}

