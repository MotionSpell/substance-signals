#include "lib_media/transform/time_rectifier.hpp"
#include "tests.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_media/transform/time_rectifier.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_media/common/pcm.hpp"
#include "lib_media/common/picture.hpp"
#include <cmath>

using namespace Tests;
using namespace Modules;

namespace {
template<typename METADATA, typename PIN>
class DataGenerator : public Module, public virtual IOutputCap {
public:
	DataGenerator() {
		output = addOutput<PIN>();
		output->setMetadata(shptr(new METADATA));
	}
	void process() {}
	void push(int64_t mediaTime, int64_t clockTime) {
		auto data = output->getBuffer(0);
		data->setMediaTime(mediaTime);
		data->setClockTime(clockTime);
		output->emit(data);
	}

private:
	PIN *output;
};

template<typename Metadata, typename PinType>
void testRectifier(const Fraction &fps, const std::vector<std::pair<int64_t, int64_t>> &inTimes, const std::vector<std::pair<int64_t, int64_t>> &outTimes) {
	auto clock = shptr(new Clock(0.0));
	auto rectifier = createModule<TimeRectifier>(1, clock, fps);
	auto generator = createModule<DataGenerator<Metadata, PinType>>(inTimes.size(), clock);
	auto executor = uptr(new Signals::ExecutorThread<void()>(""));
	ConnectModules(generator.get(), 0, rectifier.get(), 0, *executor);
	auto recorder = create<Utils::Recorder>();
	ConnectModules(rectifier.get(), 0, recorder.get(), 0);
	for (size_t i = 0; i < inTimes.size(); ++i) {
		generator->push(inTimes[i].first, inTimes[i].second);
	}
	rectifier->flush();
	recorder->process(nullptr);
	Data data;
	size_t i = 0, iMax = std::min<size_t>(inTimes.size(), outTimes.size());
	while ((data = recorder->pop()) && (i < iMax)) {
		Log::msg(Debug, "recv %s-%s (expected %s-%s)", data->getMediaTime(), data->getClockTime(), outTimes[i].first, outTimes[i].second);
		ASSERT(llabs(data->getMediaTime() == outTimes[i].first));
		ASSERT(llabs(data->getClockTime() == outTimes[i].second));
		i++;
	}
	ASSERT(i == iMax);
}

auto const generateValuesDefault = [](uint64_t step, Fraction fps) {
	auto const t = (Clock::Rate * step * fps.den) / fps.num;
	return std::pair<int64_t, int64_t>(t, t);
};

std::vector<std::pair<int64_t, int64_t>> generateData(Fraction fps, const std::function<std::pair<int64_t, int64_t>(uint64_t, Fraction)> &generateValue = generateValuesDefault) {
	auto const numItems = (size_t)(Fraction(15) * fps / Fraction(25, 1));
	std::vector<std::pair<int64_t, int64_t>> times; times.resize(numItems);
	for (size_t i = 0; i < numItems; ++i) {
		times[i] = generateValue(i, fps);
	}
	return times;
}

void testFPSFactor(const Fraction &fps, const Fraction &factor) {
	auto const genVal = [&](uint64_t step, Fraction fps) {
		auto const tIn = timescaleToClock(step * fps.den, fps.num);
		auto const stepOutIn180k = (Clock::Rate * fps.den * factor.num) / (fps.num * factor.den);
		auto const tOut = tIn / stepOutIn180k * stepOutIn180k;
		return std::pair<int64_t, int64_t>(tIn, tOut);
	};

	auto const outTimes = generateData(fps * factor, genVal);
	auto const inTimes = generateData(fps);
	testRectifier<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps * factor, inTimes, outTimes);
}
}

unittest("rectifier: FPS factor with a single pin") {
	auto const FPSs = { Fraction(25, 1), Fraction(30000, 1001) };
	auto const factors = { Fraction(1, 1), Fraction(2, 1), Fraction(1, 2) };
	for (auto &fps : FPSs) {
		for (auto &factor : factors) {
			Log::msg(Info, "Testing FPS %s/%s with output factor %s/%s", fps.num, fps.den, factor.num, factor.den);
			testFPSFactor(fps, factor);
		}
	}
}

#if 0
unittest("rectifier: initial offset") {
	auto const fps = Fraction(25, 1);
	auto const inGenVal = [&](uint64_t step, Fraction fps, int shift) {
		auto const t = (int64_t)(Clock::Rate * (step + shift) * fps.den) / fps.num;
		return std::pair<int64_t, int64_t >(t, t);
	};

	auto const outTimes = generateData(fps);
	auto const inTimes1 = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2,  5));
	testRectifier<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);

	auto const inTimes2 = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2, -5));
	testRectifier<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);
}

//missing frame: 1 2   4 5  => 1 2 2 4 5
unittest("rectifier: deal with gaps") {
	const uint64_t freq = 2;
	auto const fps = Fraction(25, 1);

	auto const inGenVal = [&](uint64_t step, Fraction fps) {
		static uint64_t i = 0;
		if (step && !(step % freq)) i++;
		auto const t = (Clock::Rate * (step+i) * fps.den) / fps.num;
		return std::pair<int64_t, int64_t >(t, t);
	};
	auto const inTimes = generateData(fps, inGenVal);

	auto const outGenVal = [&](uint64_t step, Fraction fps) {
		auto const t = (Clock::Rate * step * fps.den) / fps.num;
		static uint64_t prevT = 0, i = 0;
		const uint64_t val = (i && !(i % freq)) ? prevT : t;
		i++; prevT = t;
		return std::pair<int64_t, int64_t >(val, (Clock::Rate * step * fps.den) / fps.num);
	};
	auto const outTimes = generateData(fps, outGenVal);

	testRectifier<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes, outTimes);
}

//ts backward  : 1 2 10 11 => 1 2 3 4
unittest("rectifier: deal with backward discontinuity") {
	assert(0);
}

unittest("rectifier: multiple pins") {
	assert(0);
}

unittest("rectifier: fail when no video") {
	bool thrown = false;
	try {
		testRectifier<MetadataRawAudio, OutputPcm>(Fraction(25, 1), { { 0, 0 } }, { { 0, 0 } });
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}
#endif

unittest("restamp: passthru with offsets") {
	auto const time = 10001LL;
	auto data = std::make_shared<DataRaw>(0);

	data->setMediaTime(time);
	auto restamp = create<Transform::Restamp>(Transform::Restamp::Reset);
	restamp->process(data);
	ASSERT_EQUALS(0, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Reset, 0);
	restamp->process(data);
	ASSERT_EQUALS(0, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Reset, time);
	restamp->process(data);
	ASSERT_EQUALS(time, data->getMediaTime());
}

unittest("restamp: reset with offsets") {
	int64_t time = 10001;
	int64_t offset = -100;
	auto data = std::make_shared<DataRaw>(0);

	data->setMediaTime(time);
	auto restamp = create<Transform::Restamp>(Transform::Restamp::Passthru);
	restamp->process(data);
	ASSERT_EQUALS(time, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, 0);
	restamp->process(data);
	ASSERT_EQUALS(time, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, offset);
	restamp->process(data);
	ASSERT_EQUALS(time + offset, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, time);
	restamp->process(data);
	ASSERT_EQUALS(time + time, data->getMediaTime());
}
