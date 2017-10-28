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

const double tolerance = 0.0001 * Clock::Rate;

template<typename Metadata, typename PinType>
void testRectifier(const Fraction &fps, const std::vector<std::pair<int64_t, int64_t>> &inTimes, const std::vector<int64_t> &outTimes) {
	auto clock = shptr(new Clock(0.0));
	auto rectifier = createModule<TimeRectifier>(1, clock, fps);
	auto generator = create<DataGenerator<Metadata, PinType>>();
	ConnectModules(generator.get(), 0, rectifier.get(), 0);
	auto recorder = create<Utils::Recorder>();
	ConnectModules(rectifier.get(), 0, recorder.get(), 0);
	for (size_t i = 0; i < inTimes.size(); ++i) {
		generator->push(inTimes[i].first, inTimes[i].second);
	}
	rectifier->flush();
	recorder->process(nullptr);
	Data data;
	size_t i = 0;
	while ((data = recorder->pop()) && (i < outTimes.size())) {
		Log::msg(Debug, "recv %s (expected %s)", data->getMediaTime(), outTimes[i]);
		ASSERT(fabs(data->getMediaTime() - outTimes[i++]) <= tolerance);
	}
	ASSERT(i == outTimes.size());
}

template<typename T>
std::vector<T> generateData(Fraction fps, const std::function<T(uint64_t)> &generateValue = [](uint64_t t) { return t; }) {
	auto const numItems = (size_t)(Fraction(20) * fps / Fraction(25, 1));
	std::vector<T> times; times.resize(numItems);
	for (size_t i = 0; i < numItems; ++i) {
		const uint64_t val = (Clock::Rate * i * fps.den) / fps.num;
		times[i] = generateValue(val);
	}
	return times;
}

void testFPSFactor(const Fraction &fps, const Fraction &factor) {
	auto const inGenVal = [](uint64_t t) { return std::pair<int64_t, int64_t>(t, t); };
	auto const inTimes = generateData<std::pair<int64_t, int64_t>>(fps, inGenVal);
	auto const outTimes = generateData<int64_t>(fps * factor);
	testRectifier<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps * factor, inTimes, outTimes);
}

auto const FPSs    = { /*Fraction(25, 1),*/ Fraction(30000, 1001) };
auto const factors = { /*Fraction(1, 1), Fraction(2, 1), Fraction(1, 2), Fraction(25 * 1001, 30000),*/ Fraction(30000, 25 * 1001) };
}

unittest("rectifier: FPS factor with a single pin") {
	for (auto &fps : FPSs) {
		for (auto &factor : factors) {
			Log::msg(Info, "Testing FPS %s/%s with output factor %s/%s", fps.num, fps.den, factor.num, factor.den);
			testFPSFactor(fps, factor);
		}
	}
}

unittest("rectifier: multiple pins") {
}

//missing frame: 1 2 4  5  => 1 2 2 4 5
unittest("rectifier: deal with gaps") {
}

//ts backward  : 1 2 10 11 => 1 2 3 4
unittest("rectifier: deal with backward discontinuity") {
}

unittest("rectifier: fail when no video") {
	bool thrown = false;
	try {
		testRectifier<MetadataRawAudio, OutputPcm>(Fraction(25, 1), { { 0, 0 } }, { 0 });
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}

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
