#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/transform/audio_gap_filler.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_utils/tools.hpp"
#include "lib_utils/log.hpp"
#include <cassert>

using namespace Tests;
using namespace Modules;
using namespace std;

namespace {
shared_ptr<DataPcm> getInterleavedPcmData() {

	auto fmt = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);

	static const short data[] = {
		// L,R
		40, 80,
		41, 81,
		42, 82,
		43, 83,
		44, 84,
		45, 85,
		46, 86,
		47, 87,
	};

	auto r = make_shared<DataPcm>(0);
	r->setFormat(fmt);
	r->setPlane(0, (uint8_t*)data, sizeof data);
	r->setMetadata(make_shared<MetadataRawAudio>());
	return r;
}

vector<short> getPlane(DataPcm const* data, int idx) {
	auto ptr = (short*)data->getPlane(idx);
	auto len = (size_t)(data->getPlaneSize(idx) / sizeof(short));
	return vector<short>(ptr, ptr + len);
}

struct Recorder : ModuleS {
	Recorder() {
		addInput(this);
	}
	void process(Data frame_) override {
		out = frame_;
	}
	Data out;
};
}

unittest("audio converter: interleaved to planar") {
	auto in = getInterleavedPcmData();
	auto planar = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Planar);

	auto cfg = AudioConvertConfig { in->getFormat(), planar, -1 };
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);

	auto rec = createModule<Recorder>();
	ConnectOutputToInput(converter->getOutput(0), rec->getInput(0));
	converter->getInput(0)->push(in);
	converter->process();
	converter->flush();

	auto out = safe_cast<const DataPcm>(rec->out);
	ASSERT_EQUALS(2, (int)out->getFormat().numPlanes);
	ASSERT_EQUALS(vector<short>({40, 41, 42, 43, 44, 45, 46, 47 }), getPlane(out.get(), 0));
	ASSERT_EQUALS(vector<short>({80, 81, 82, 83, 84, 85, 86, 87 }), getPlane(out.get(), 1));
}

unittest("audio converter: multiple flushes while upsampling") {
	auto const srcFormat = PcmFormat(32000, 1, Mono, S16, Interleaved);
	auto const dstFormat = PcmFormat(48000, 1, Mono, S16, Interleaved);
	auto const dstSamples = 1152;

	AudioConvertConfig cfg { srcFormat, dstFormat, dstSamples };

	auto converter = loadModule("AudioConvert", &NullHost, &cfg);

	int outputSize = 0;

	auto onFrame = [&](Data data) {
		ASSERT_EQUALS(dstSamples * dstFormat.getBytesPerSample(), (int)data->data().len);
		outputSize += data->data().len;
	};

	ConnectOutput(converter.get(), onFrame);

	int inputSize = 0;
	std::vector<uint8_t> buf(3110400);

	auto data = make_shared<DataPcm>(0);
	data->setFormat(srcFormat);
	data->setPlane(0, buf.data(), buf.size());

	inputSize += buf.size();
	converter->getInput(0)->push(data);
	converter->process();

	inputSize += buf.size();
	converter->getInput(0)->push(data);
	converter->process();

	converter->flush();

	inputSize += buf.size();
	converter->getInput(0)->push(data);
	converter->process();

	converter->flush();

	ASSERT_EQUALS(divUp(inputSize, srcFormat.sampleRate), divUp(outputSize, dstFormat.sampleRate));
}

unittest("audio converter: 44100 to 48000") {
	auto in = getInterleavedPcmData();
	auto dstFormat = PcmFormat(48000, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);

	auto cfg = AudioConvertConfig { in->getFormat(), dstFormat, -1 };
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);

	vector<short> output;
	auto onFrame = [&](Data data) {
		auto const v = getPlane(safe_cast<const DataPcm>(data).get(), 0);
		output.insert(output.end(), v.begin(), v.end());
	};

	ConnectOutput(converter.get(), onFrame);
	for(int i=0; i < 3; ++i) {
		converter->getInput(0)->push(in);
		converter->process();
	}
	converter->flush();

	short expected[] = {
		// L, R
		40, 80,
		41, 81,
		42, 82,
		43, 83,
		43, 83,
		45, 85,
		45, 85,
		47, 87,
		45, 85,
		39, 79,
		42, 82,
		42, 82,
		43, 83,
		44, 84,
		45, 85,
		45, 85,
		48, 88,
		43, 83,
		39, 79,
		42, 82,
		42, 82,
		44, 84,
		0,  0,
		0,  0,
		0,  0,
		0,  0,
		0,  0,
	};
	auto const expectedNumEntries = sizeof(expected) / sizeof(short);
	ASSERT_EQUALS(vector<short>(expected, expected + expectedNumEntries), output);
}

#include "lib_media/in/sound_generator.hpp"

unittest("audio converter: dynamic formats") {
	auto soundGen = createModule<In::SoundGenerator>(&NullHost);
	auto recorder = createModule<Utils::Recorder>(&NullHost);

	PcmFormat format;
	auto cfg = AudioConvertConfig { {0}, format, -1 };
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);

	ConnectOutputToInput(soundGen->getOutput(0), converter->getInput(0));
	ConnectOutputToInput(converter->getOutput(0), recorder->getInput(0));

	soundGen->process();
	soundGen->process();

	converter->flush();
	recorder->process(nullptr);

	while (auto data = recorder->pop()) {
		converter->getInput(0)->push(data);
		converter->process();
	}
}

namespace {
void framingTest(int inSamplesPerFrame, int outSamplesPerFrame) {
	auto format = PcmFormat(44100, 2, Stereo, S16, Planar);

	const auto inFrameSize = inSamplesPerFrame * format.getBytesPerSample() / format.numPlanes;
	auto data = make_shared<DataPcm>(0);
	data->setFormat(format);

	std::vector<uint8_t> input(inFrameSize);
	const int modulo = std::min(inFrameSize, 256);
	for (int i = 0; i < inFrameSize; ++i) {
		input[i] = (uint8_t)(i % modulo);
	}
	for (int i = 0; i < format.numPlanes; ++i) {
		data->setPlane(i, input.data(), inFrameSize);
	}

	int outTotalSize = 0;

	int val = 0;
	auto const numIter = 3;
	auto onFrame = [&](Data dataRec) {
		auto data = safe_cast<const DataPcm>(dataRec);

		std::vector<int> expected;
		auto const planeSize = (int)data->getPlaneSize(0);
		for (int i = 0; i < planeSize; ++i) {
			if (val < numIter * inFrameSize) {
				expected.push_back((val % inFrameSize) % modulo);
				++val;
			} else {
				expected.push_back(0);
			}
		}

		for (int p = 0; p < data->getFormat().numPlanes; ++p) {
			auto const plane = data->getPlane(p);
			ASSERT_EQUALS(data->getPlaneSize(0), data->getPlaneSize(p));
			auto const maxAllowedSize = outSamplesPerFrame * format.getBytesPerSample() / format.numPlanes;
			ASSERT_EQUALS(maxAllowedSize, max(planeSize, maxAllowedSize));

			std::vector<int> actual;
			for (int s = 0; s < planeSize; ++s)
				actual.push_back(plane[s]);

			ASSERT_EQUALS(expected, actual);

			outTotalSize += planeSize;
		}
	};

	auto cfg = AudioConvertConfig { format, format, (int64_t)outSamplesPerFrame};
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);
	ConnectOutput(converter.get(), onFrame);

	for (int i = 0; i < numIter; ++i) {
		converter->getInput(0)->push(data);
		converter->process();
	}
	converter->flush();

	const int expectedTotalSize = divUp(inSamplesPerFrame * numIter, outSamplesPerFrame) * outSamplesPerFrame * format.getBytesPerSample();
	ASSERT_EQUALS(expectedTotalSize, outTotalSize);
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
	framingTest(2, 1);
}

unittest("audio converter: bigger framing size.") {
	framingTest(1024, 1152);
	framingTest(1024, 4096);
}

unittest("audio converter: timestamp passthrough") {
	auto format = PcmFormat(44100, 1, Mono, S16, Planar);

	int64_t lastMediaTime = 0;

	auto onFrame = [&](Data dataRec) {
		lastMediaTime = dataRec->getMediaTime();
	};

	auto cfg = AudioConvertConfig { format, format, (int64_t)1024};
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);
	ConnectOutput(converter.get(), onFrame);

	auto data = make_shared<DataPcm>(0);
	data->setMediaTime(777777);
	data->setFormat(format);
	data->setPlane(0, nullptr, 1024);
	converter->getInput(0)->push(data);
	converter->process();
	converter->flush();

	// FIXME: remove '-3', which works around a bug
	ASSERT_EQUALS(777777, lastMediaTime - 3);
}

unittest("[DISABLED] audio converter: timestamp gap") {
	const auto inSamplesPerFrame = 2000;
	const auto outSamplesPerFrame = 1024;
	auto format = PcmFormat(44100, 1, Mono, S16, Planar);

	auto createSample = [&](int64_t mediaTime) {
		const auto inFrameSize = inSamplesPerFrame * format.getBytesPerSample();

		auto data = make_shared<DataPcm>(0);
		data->setMediaTime(mediaTime);
		data->setFormat(format);
		data->setPlane(0, nullptr, inFrameSize);
		return data;
	};

	int64_t lastMediaTime = 0;

	auto onFrame = [&](Data dataRec) {
		lastMediaTime = dataRec->getMediaTime();
	};

	auto cfg = AudioConvertConfig { format, format, (int64_t)outSamplesPerFrame};
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);
	ConnectOutput(converter.get(), onFrame);

	int64_t const timeAfterGap = 1000 * 1000;

	{
		auto data = createSample(0);
		converter->getInput(0)->push(data);
		converter->process();
	}

	{
		auto data = createSample(timeAfterGap);
		converter->getInput(0)->push(data);
		converter->process();
	}

	ASSERT_EQUALS(timeAfterGap, std::min(lastMediaTime, timeAfterGap));

	converter->flush();
}

unittest("audio gap filler") {
	PcmFormat format;
	auto data = make_shared<DataPcm>(0);
	data->setFormat(format);
	auto const numSamples = 1024;
	const size_t inFrameSizeInBytes = numSamples * format.getBytesPerSample() / format.numPlanes;
	std::vector<uint8_t> input(inFrameSizeInBytes);
	for (int i = 0; i < format.numPlanes; ++i) {
		data->setPlane(i, input.data(), inFrameSizeInBytes);
	}

	const std::vector<int64_t> in =  { 1, 2, 3,    5, 6, 7, 8, 7, 8, 9, 1000, 1001, 1002, 3, 4, 5 };
	const std::vector<int64_t> out = { 1, 2, 3, 4, 5, 6, 7, 8,       9, 1000, 1001, 1002, 3, 4, 5 };
	auto recorder = createModule<Utils::Recorder>(&NullHost);
	auto gapFiller = createModule<Transform::AudioGapFiller>(&NullHost, out.size());
	ConnectOutputToInput(gapFiller->getOutput(0), recorder->getInput(0));
	for (auto &val : in) {
		data->setMediaTime(val * numSamples, format.sampleRate);
		gapFiller->process(data);
	}
	recorder->process(nullptr);

	size_t idx = 0;
	while (auto dataRec = recorder->pop()) {
		ASSERT(std::abs(dataRec->getMediaTime() - timescaleToClock(out[idx] * numSamples, format.sampleRate)) < 6);
		idx++;
	}
	ASSERT(idx == out.size());
}

