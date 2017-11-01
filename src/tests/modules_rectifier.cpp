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

class ClockMock : public IClock {
public:
	virtual ~ClockMock() {
		condition.notify_all();
	}
	void setTime(const Fraction &t) {
		time = t;
		condition.notify_all();
	}

	Fraction now() const override {
		return time;
	}
	double getSpeed() const override {
		return 0.0;
	}
	void sleep(Fraction t) const override {
		std::unique_lock<std::mutex> lock(mutex);
		auto const tInit = time;
		while (time < tInit + t) {
			auto const durInMs = std::chrono::milliseconds(10);
			condition.wait_for(lock, durInMs);
		}
	}

private:
	Fraction time = 0;
	mutable std::mutex mutex;
	mutable std::condition_variable condition;
};

template<typename METADATA, typename PIN>
class DataGenerator : public ModuleS, public virtual IOutputCap {
public:
	DataGenerator() {
		output = addOutput<PIN>();
		output->setMetadata(shptr(new METADATA));
	}
	void process(Data data) {
		auto dataOut = output->getBuffer(0);
		dataOut->setMediaTime(data->getMediaTime());
		dataOut->setClockTime(data->getClockTime());
		output->emit(dataOut);
	}

private:
	PIN *output;
};

void testRectifierMeta(const Fraction &fps, std::shared_ptr<ClockMock> clock,
	const std::vector<std::unique_ptr<ModuleS>> &generators,
	const std::vector<std::vector<std::pair<int64_t, int64_t>>> &inTimes, const std::vector<std::vector<std::pair<int64_t, int64_t>>> &outTimes,
	bool async = true) {
	auto rectifier = createModule<TimeRectifier>(1, clock, fps);
	auto executor = uptr(new Signals::ExecutorThread<void()>(""));
	if (async) {
		ConnectModules(generators[0].get(), 0, rectifier.get(), 0, *executor);
	} else {
		ConnectModules(generators[0].get(), 0, rectifier.get(), 0);
	}
	auto recorder = create<Utils::Recorder>();
	ConnectModules(rectifier.get(), 0, recorder.get(), 0);

	{
		std::shared_ptr<DataRaw> data(new DataRaw(0));
		for (size_t i = 0; i < inTimes[0].size(); ++i) {
			data->setMediaTime(inTimes[0][i].first);
			data->setClockTime(inTimes[0][i].second);
			generators[0]->process(data);
			clock->setTime(Fraction(inTimes[0][i].second, Clock::Rate));
		}
	}
	(*executor)([&]() {
		auto const t = inTimes[0][inTimes[0].size()-1].second + 1;
		Log::msg(Debug, "Set clock to final value: %s", t);
		clock->setTime(Fraction(t, Clock::Rate));
	});
	rectifier->flush();

	recorder->process(nullptr);
	size_t i = 0, iMax = std::min<size_t>(inTimes[0].size(), outTimes[0].size());
	{
		Data data;
		while ((data = recorder->pop()) && (i < iMax)) {
			Log::msg(Debug, "recv %s-%s (expected %s-%s)", data->getMediaTime(), data->getClockTime(), outTimes[0][i].first, outTimes[0][i].second);
			ASSERT(llabs(data->getMediaTime() == outTimes[0][i].first));
			ASSERT(llabs(data->getClockTime() == outTimes[0][i].second));
			i++;
		}
	}
	ASSERT(i == iMax);
	clock->setTime(std::numeric_limits<int32_t>::max());
}

template<typename Metadata, typename PinType>
void testRectifierSinglePin(const Fraction &fps, const std::vector<std::pair<int64_t, int64_t>> &inTimes, const std::vector<std::pair<int64_t, int64_t>> &outTimes, bool async = true) {
	std::vector<std::vector<std::pair<int64_t, int64_t>>> in;
	in.push_back(inTimes);
	std::vector<std::vector<std::pair<int64_t, int64_t>>> out;
	out.push_back(outTimes);
	std::vector<std::unique_ptr<ModuleS>> generators;
	auto clock = shptr(new ClockMock);
	generators.push_back(createModule<DataGenerator<Metadata, PinType>>(in[0].size(), clock));
	testRectifierMeta(fps, clock, generators, in, out, async);
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
	testRectifierSinglePin<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps * factor, inTimes, outTimes);
}
}

unittest("scheduler: mock clock") {
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
	};

	{
		auto const f1 = Fraction(1, 1000);
		auto const f10 = Fraction(10, 1000);
		auto clock = shptr(new ClockMock);
		Scheduler s(clock);
		s.scheduleIn(f, f1);
		g_DefaultClock->sleep(f10);
		ASSERT(q.size() == 0);
		clock->setTime(f10);
		g_DefaultClock->sleep(f10);
		ASSERT(q.size() == 1);
		auto const t = q.pop();
		ASSERT(t == f1);
	}
}

unittest("rectifier: FPS factor (single pin)") {
	auto const FPSs = { Fraction(25, 1), Fraction(30000, 1001) };
	auto const factors = { Fraction(1, 1), Fraction(2, 1), Fraction(1, 2) };
	for (auto &fps : FPSs) {
		for (auto &factor : factors) {
			Log::msg(Info, "Testing FPS %s/%s with output factor %s/%s", fps.num, fps.den, factor.num, factor.den);
			testFPSFactor(fps, factor);
		}
	}
}

unittest("rectifier: initial offset (single pin)") {
	auto const fps = Fraction(25, 1);
	auto const inGenVal = [&](uint64_t step, Fraction fps, int shift) {
		auto const t = (int64_t)(Clock::Rate * (step + shift) * fps.den) / fps.num;
		return std::pair<int64_t, int64_t >(t, (Clock::Rate * step * fps.den) / fps.num);
	};

	auto const outTimes = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2, 0));
	auto const inTimes1 = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2, 5));
	testRectifierSinglePin<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);

	auto const inTimes2 = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2, -5));
	testRectifierSinglePin<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);
}

unittest("rectifier: deal with missing frames (single pin)") {
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
		static uint64_t prevT = 0, i = 1;
		const uint64_t val = !(i % (freq+1)) ? prevT : t;
		i++; prevT = t;
		return std::pair<int64_t, int64_t >((Clock::Rate * step * fps.den) / fps.num, val);
	};
	auto const outTimes = generateData(fps, outGenVal);

	testRectifierSinglePin<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes, outTimes);
}

unittest("rectifier: deal with backward discontinuity (single pin)") {
	auto const fps = Fraction(25, 1);
	auto const u = fractionToClock(fps);
	auto const outGenVal = [&](uint64_t step, Fraction fps, int64_t clockTimeOffset, int64_t mediaTimeOffset) {
		auto const mediaTime = (int64_t)(Clock::Rate * (step + mediaTimeOffset) * fps.den) / fps.num;
		auto const clockTime = (int64_t)(Clock::Rate * (step + clockTimeOffset) * fps.den) / fps.num;
		return std::pair<int64_t, int64_t >(mediaTime, clockTime);
	};
	auto inTimes1 = generateData(fps);
	auto inTimes2 = generateData(fps, std::bind(outGenVal, std::placeholders::_1, std::placeholders::_2, inTimes1.size(), 0));
	auto outTimes1 = generateData(fps);
	auto outTimes2 = generateData(fps, std::bind(outGenVal, std::placeholders::_1, std::placeholders::_2, inTimes1.size(), inTimes1.size()));
	inTimes1.insert(inTimes1.end(), inTimes2.begin(), inTimes2.end());
	outTimes1.insert(outTimes1.end(), outTimes2.begin(), outTimes2.end());
	testRectifierSinglePin<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes1);
}

#if 0
unittest("rectifier: multiple media types") {
	assert(0);
}
#endif

unittest("rectifier: fail when no video") {
	bool thrown = false;
	try {
		testRectifierSinglePin<MetadataRawAudio, OutputPcm>(Fraction(25, 1), { { 0, 0 } }, { { 0, 0 } }, false);
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
