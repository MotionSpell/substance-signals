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

	auto out = safe_cast<const DataPcm>(rec->out);
	ASSERT_EQUALS(2, (int)out->getFormat().numPlanes);
	ASSERT_EQUALS(vector<short>({40, 41, 42, 43, 44, 45, 46, 47 }), getPlane(out.get(), 0));
	ASSERT_EQUALS(vector<short>({80, 81, 82, 83, 84, 85, 86, 87 }), getPlane(out.get(), 1));
}

unittest("audio converter: 44100 to 48000") {

	auto in = getInterleavedPcmData();
	auto dstFormat = PcmFormat(48000, 2, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved);

	auto cfg = AudioConvertConfig { in->getFormat(), dstFormat, -1 };
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);

	auto rec = createModule<Recorder>();
	ConnectOutputToInput(converter->getOutput(0), rec->getInput(0));
	for(int i=0; i < 3; ++i) {
		converter->getInput(0)->push(in);
		converter->process();
	}
	converter->flush();

	assert(rec->out);
	auto out = safe_cast<const DataPcm>(rec->out);
	ASSERT_EQUALS(1, (int)out->getFormat().numPlanes);
	short expected[] = {
		// L, R
		39, 79,
		42, 82,
		42, 82,
		43, 83,
		44, 84,
		45, 85,
		45, 85,
		48, 88,
		43, 83,
	};
	ASSERT_EQUALS(vector<short>(expected, expected+18), getPlane(out.get(), 0));
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
void framingTest(const size_t inFrameFrames, const size_t outFrameFrames) {
	PcmFormat format;
	const size_t inFrameSizeInBytes = inFrameFrames * format.getBytesPerSample() / format.numPlanes;
	auto data = make_shared<DataPcm>(0);
	data->setFormat(format);

	std::vector<uint8_t> input(inFrameSizeInBytes);
	auto inputRaw = input.data();
	const size_t modulo = std::min<size_t>(inFrameSizeInBytes, 256);
	for (size_t i = 0; i < inFrameSizeInBytes; ++i) {
		inputRaw[i] = (uint8_t)(i % modulo);
	}
	for (int i = 0; i < format.numPlanes; ++i) {
		data->setPlane(i, inputRaw, inFrameSizeInBytes);
	}

	auto recorder = createModule<Utils::Recorder>(&NullHost);
	auto cfg = AudioConvertConfig { format, format, (int64_t)outFrameFrames};
	auto converter = loadModule("AudioConvert", &NullHost, &cfg);
	ConnectOutputToInput(converter->getOutput(0), recorder->getInput(0));

	auto const numIter = 3;
	for (size_t i = 0; i < numIter; ++i) {
		data->setMediaTime(inFrameFrames * i, format.sampleRate);
		converter->getInput(0)->push(data);
		converter->process();
	}
	converter->flush();
	recorder->process(nullptr);

	size_t idx = 0;
	while (auto dataRec = recorder->pop()) {
		auto audioData = safe_cast<const DataPcm>(dataRec);
		size_t val = 0;
		for (int p = 0; p < audioData->getFormat().numPlanes; ++p) {
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

