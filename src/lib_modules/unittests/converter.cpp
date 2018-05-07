#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/in/sound_generator.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_media/transform/audio_gap_filler.hpp"
#include "lib_media/transform/video_convert.hpp"
#include "lib_media/utils/comparator.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_utils/profiler.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

namespace {

unittest("audio converter: interleaved to planar to interleaved") {
	auto soundGen = create<In::SoundGenerator>();
	auto comparator = create<Utils::PcmComparator>();
	Connect(soundGen->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOriginal);

	auto baseFormat  = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto otherFormat = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Planar);

	auto converter1 = create<Transform::AudioConvert>(baseFormat, otherFormat);
	auto converter2 = create<Transform::AudioConvert>(otherFormat, baseFormat);

	ConnectOutputToInput(soundGen->getOutput(0), converter1->getInput(0));
	ConnectOutputToInput(converter1->getOutput(0), converter2->getInput(0));
	Connect(converter2->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOther);

	soundGen->process(nullptr);
	comparator->process(nullptr);
}

unittest("audio converter: 44100 to 48000") {
	auto soundGen = create<In::SoundGenerator>();
	auto comparator = create<Utils::PcmComparator>();
	Connect(soundGen->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOriginal);

	auto baseFormat  = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto otherFormat = PcmFormat(48000, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);
	auto converter1 = create<Transform::AudioConvert>(baseFormat, otherFormat);
	auto converter2 = create<Transform::AudioConvert>(otherFormat, baseFormat);

	ConnectOutputToInput(soundGen->getOutput(0), converter1->getInput(0), &g_executorSync);
	ConnectOutputToInput(converter1->getOutput(0), converter2->getInput(0), &g_executorSync);
	Connect(converter2->getOutput(0)->getSignal(), comparator.get(), &Utils::PcmComparator::pushOther);

	soundGen->process(nullptr);

	converter1->flush();
	converter2->flush();
	comparator->process(nullptr);
}

unittest("audio converter: dynamic formats") {
	auto soundGen = create<In::SoundGenerator>();
	auto recorder = create<Utils::Recorder>();

	PcmFormat format;
	auto converter = create<Transform::AudioConvert>(format);

	ConnectOutputToInput(soundGen->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), recorder->getInput(0));

	soundGen->process(nullptr);

	{
		Tools::Profiler profilerGlobal("  Send to converter");
		soundGen->process(nullptr);
	}

	converter->flush();
	converter->getOutput(0)->getSignal().disconnect(0);
	recorder->process(nullptr);

	{
		Tools::Profiler profilerGlobal("  Passthru");
		Data data;
		while ((data = recorder->pop())) {
			converter->process(data);
		}
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

	auto recorder = create<Utils::Recorder>();
	auto converter = create<Transform::AudioConvert>(format, format, outFrameFrames);
	ConnectOutputToInput(converter->getOutput(0), recorder->getInput(0));

	auto const numIter = 3;
	for (size_t i = 0; i < numIter; ++i) {
		data->setMediaTime(inFrameFrames * i, format.sampleRate);
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
	framingTest(1024, 1024);
	framingTest(111, 111);
	framingTest(11, 11);
	framingTest(1, 1);
}

unittest("audio converter: smaller framing size.") {
	framingTest(1152, 1024);
	framingTest(1152, 512);
}

unittest("audio converter: bigger framing size.") {
	framingTest(1024, 1152);
	framingTest(1024, 4096);
}

unittest("video converter: pass-through") {
	auto const res = Resolution(16, 32);
	auto const format = PictureFormat(res, YUV420P);
	int numFrames = 0;

	auto onFrame = [&](Data data) {
		auto pic = safe_cast<const DataPicture>(data);
		ASSERT(pic->getFormat() == format);
		numFrames++;
	};

	{
		auto convert = create<Transform::VideoConvert>(format);
		Connect(convert->getOutput(0)->getSignal(), onFrame);

		std::shared_ptr<DataBase> pic = uptr(new PictureYUV420P(res));
		convert->process(pic);
	}

	ASSERT_EQUALS(1, numFrames);
}

unittest("video converter: different sizes") {
	auto const srcRes = Resolution(16, 32);
	auto const dstRes = Resolution(24, 8);
	auto const format = PictureFormat(dstRes, YUV420P);
	int numFrames = 0;

	auto onFrame = [&](Data data) {
		auto pic = safe_cast<const DataPicture>(data);
		ASSERT(pic->getFormat() == format);
		numFrames++;
	};

	{
		auto convert = create<Transform::VideoConvert>(format);
		Connect(convert->getOutput(0)->getSignal(), onFrame);

		std::shared_ptr<DataBase> pic = uptr(new PictureYUV420P(srcRes));
		convert->process(pic);
	}

	ASSERT_EQUALS(1, numFrames);
}

unittest("audio gap filler") {
	PcmFormat format;
	auto data = std::make_shared<DataPcm>(0);
	data->setFormat(format);
	auto const numSamples = 1024;
	const size_t inFrameSizeInBytes = numSamples * format.getBytesPerSample() / format.numPlanes;
	std::vector<uint8_t> input(inFrameSizeInBytes);
	for (uint8_t i = 0; i < format.numPlanes; ++i) {
		data->setPlane(i, input.data(), inFrameSizeInBytes);
	}

	const std::vector<int64_t> in =  { 1, 2, 3, 5,    6, 7, 8, 9, 10, 11, 12, 12, 12, 1000, 1001, 1002, 3, 4, 5 };
	const std::vector<int64_t> out = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1000, 1001, 1002, 3, 4, 5 };
	auto recorder = create<Utils::Recorder>();
	auto gapFiller = createModule<Transform::AudioGapFiller>(out.size(), g_DefaultClock);
	ConnectOutputToInput(gapFiller->getOutput(0), recorder->getInput(0));
	for (auto &val : in) {
		data->setMediaTime(val * numSamples, format.sampleRate);
		gapFiller->process(data);
	}
	recorder->process(nullptr);

	Data dataRec;
	size_t idx = 0;
	while ((dataRec = recorder->pop())) {
		Log::msg(Debug, " %s - %s", dataRec->getMediaTime(), timescaleToClock(out[idx] * numSamples, format.sampleRate));
		ASSERT(std::abs(dataRec->getMediaTime() - timescaleToClock(out[idx] * numSamples, format.sampleRate)) < 6);
		idx++;
	}
	ASSERT(idx == out.size());

}

}
