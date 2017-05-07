#include "tests.hpp"
#include "lib_modules/modules.hpp"

#include "lib_media/in/sound_generator.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_media/transform/video_convert.hpp"
#include "lib_media/utils/comparator.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_utils/profiler.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

unittest("audio converter: interleaved to planar to interleaved") {
	auto soundGen = uptr(create<In::SoundGenerator>());
	auto comparator = uptr(create<Utils::PcmComparator>());
	Connect(soundGen->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOriginal);

	auto baseFormat  = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto otherFormat = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Planar);

	auto converter1 = uptr(create<Transform::AudioConvert>(baseFormat, otherFormat));
	auto converter2 = uptr(create<Transform::AudioConvert>(otherFormat, baseFormat));

	ConnectOutputToInput(soundGen->getOutput(0), converter1);
	ConnectOutputToInput(converter1->getOutput(0), converter2);
	Connect(converter2->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOther);

	soundGen->process(nullptr);
	SLEEP_IN_MS(200); // HACK: allow time for the data to reach the comparator ...
	bool thrown = false;
	try {
		comparator->process(nullptr);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(!thrown);
}

unittest("audio converter: 44100 to 48000") {
	auto soundGen = uptr(create<In::SoundGenerator>());
	auto comparator = uptr(create<Utils::PcmComparator>());
	Connect(soundGen->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOriginal);

	auto baseFormat  = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto otherFormat = PcmFormat(48000, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto converter1 = uptr(create<Transform::AudioConvert>(baseFormat, otherFormat));
	auto converter2 = uptr(create<Transform::AudioConvert>(otherFormat, baseFormat));

	ConnectOutputToInput(soundGen->getOutput(0), converter1, g_executorSync);
	ConnectOutputToInput(converter1->getOutput(0), converter2, g_executorSync);
	Connect(converter2->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOther);

	soundGen->process(nullptr);

	converter1->flush();
	converter2->flush();
	bool thrown = false;
	try {
		comparator->process(nullptr);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(!thrown);
}

unittest("audio converter: dynamic formats") {
	auto soundGen = uptr(create<In::SoundGenerator>());
	auto recorder = uptr(create<Utils::Recorder>());

	PcmFormat format;;
	auto converter = uptr(create<Transform::AudioConvert>(format));

	ConnectOutputToInput(soundGen->getOutput(0), converter);
	ConnectOutputToInput(converter->getOutput(0), recorder);

	{
		bool thrown = false;
		try {
			soundGen->process(nullptr);
		} catch (std::exception const& e) {
			std::cerr << "Expected error: " << e.what() << std::endl;
			thrown = true;
		}
		ASSERT(!thrown);
	}

	{
		bool thrown = false;
		try {
			Tools::Profiler profilerGlobal("  Send to converter");
			soundGen->process(nullptr);
		} catch (std::exception const& e) {
			std::cerr << "Expected error: " << e.what() << std::endl;
			thrown = true;
		}
		ASSERT(!thrown);
	}

	converter->flush();
	converter->getOutput(0)->getSignal().disconnect(0);
	recorder->process(nullptr);

	{
		bool thrown = false;
		try {
			Tools::Profiler profilerGlobal("  Passthru");
			Data data;
			while ((data = recorder->pop())) {
				converter->process(data);
			}
		} catch (std::exception const& e) {
			std::cerr << "Expected error: " << e.what() << std::endl;
			thrown = true;
		}
		ASSERT(!thrown);
	}
}

namespace {
void framingTest(const size_t inFrameFrames, const size_t outFrameFrames) {
	PcmFormat format;
	const size_t inFrameSizeInBytes = inFrameFrames * format.getBytesPerSample() / format.numPlanes;
	auto data = std::make_shared<DataPcm>(0);
	data->setFormat(format);

	std::vector<uint8_t> input(inFrameSizeInBytes);
	auto inputRaw = input.data();
	const size_t modulo = std::min<size_t>(inFrameSizeInBytes, 256);
	for (size_t i = 0; i < inFrameSizeInBytes; ++i) {
		inputRaw[i] = (uint8_t)(i % modulo);
	}
	for (uint8_t i = 0; i < format.numPlanes; ++i) {
		data->setPlane(i, inputRaw, inFrameSizeInBytes);
	}

	auto recorder = uptr(create<Utils::Recorder>());
	auto converter = uptr(create<Transform::AudioConvert>(format, format, outFrameFrames));
	ConnectOutputToInput(converter->getOutput(0), recorder);

	auto const numIter = 3;
	for (size_t i = 0; i < numIter; ++i) {
		converter->process(data);
	}
	converter->flush();
	converter->getOutput(0)->getSignal().disconnect(0);
	recorder->process(nullptr);

	Data dataRec;
	size_t idx = 0;
	while ((dataRec = recorder->pop())) {
		auto audioData = safe_cast<const DataPcm>(dataRec);
		size_t val = 0;
		for (size_t p = 0; p < audioData->getFormat().numPlanes; ++p) {
			auto const plane = audioData->getPlane(p);
			auto const planeSizeInBytes = audioData->getPlaneSize(p);
			ASSERT(planeSizeInBytes <= outFrameFrames * format.getBytesPerSample() / format.numPlanes);
			for (size_t s = 0; s < planeSizeInBytes; ++s) {
				ASSERT(plane[s] == ((val % inFrameSizeInBytes) % modulo));
				++idx;
				++val;
			}
		}
	}
	ASSERT(idx == inFrameSizeInBytes * numIter * format.numPlanes);
}
}

unittest("audio converter: same framing size.") {
	bool thrown = false;
	try {
		framingTest(1024, 1024);
		framingTest(111, 111);
		framingTest(11, 11);
		framingTest(1, 1);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(!thrown);
}

unittest("audio converter: smaller framing size.") {
	bool thrown = false;
	try {
		framingTest(1152, 1024);
		framingTest(1152, 512);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(!thrown);
}

unittest("audio converter: bigger framing size.") {
	bool thrown = false;
	try {
		framingTest(1024, 1152);
		framingTest(1024, 4096);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(!thrown);
}

unittest("video converter: pass-through") {
	auto res = Resolution(16, 32);
	auto format = PictureFormat(res, YUV420P);
	int numFrames = 0;

	auto onFrame = [&](Data data) {
		auto pic = safe_cast<const DataPicture>(data);
		ASSERT(pic->getFormat() == format);
		numFrames++;
	};

	{
		auto convert = uptr(create<Transform::VideoConvert>(format));
		Connect(convert->getOutput(0)->getSignal(), onFrame);

		std::shared_ptr<DataBase> pic = uptr(new PictureYUV420P(res));
		convert->process(pic);
	}

	ASSERT_EQUALS(1, numFrames);
}

unittest("video converter: different sizes") {
	auto srcRes = Resolution(16, 32);
	auto dstRes = Resolution(24, 8);
	auto format = PictureFormat(dstRes, YUV420P);
	int numFrames = 0;

	auto onFrame = [&](Data data) {
		auto pic = safe_cast<const DataPicture>(data);
		ASSERT(pic->getFormat() == format);
		numFrames++;
	};

	{
		auto convert = uptr(create<Transform::VideoConvert>(format));
		Connect(convert->getOutput(0)->getSignal(), onFrame);

		std::shared_ptr<DataBase> pic = uptr(new PictureYUV420P(srcRes));
		convert->process(pic);
	}

	ASSERT_EQUALS(1, numFrames);
}

}
