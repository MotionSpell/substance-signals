#include "tests/tests.hpp"
#include "lib_utils/queue_inspect.hpp"
#include "lib_media/transform/time_rectifier.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_media/common/pcm.hpp"
#include "lib_media/common/picture.hpp"
#include <cmath>
#include <iostream>

using namespace std;
using namespace Tests;
using namespace Modules;

struct TimePair {
	int64_t mediaTime;
	int64_t clockTime;
};

// allows ASSERT_EQUALS on fractions
static std::ostream& operator<<(std::ostream& o, Fraction f) {
	o << f.num << "/" << f.den;
	return o;
}

// allows ASSERT_EQUALS on TimePair
static std::ostream& operator<<(std::ostream& o, TimePair t) {
	o << t.mediaTime << "-" << t.clockTime;
	return o;
}

static bool operator==(TimePair a, TimePair b) {
	return a.clockTime == b.clockTime && a.mediaTime == b.mediaTime;
}

class ClockMock : public IClock {
	public:
		ClockMock(Fraction time = Fraction(-1, 1000)) : m_time(time) {}
		void setTime(Fraction t) {
			unique_lock<std::mutex> lock(protectTime);
			if (t > m_time) {
				m_time = t;
			}
			timeChanged.notify_all();
		}

		Fraction now() const override {
			unique_lock<std::mutex> lock(protectTime);
			return m_time;
		}

		double getSpeed() const override {
			return 0.0;
		}

		void sleep(Fraction delay) const override {
			unique_lock<std::mutex> lock(protectTime);
			auto const end = m_time + delay;
			while (m_time < end) {
				timeChanged.wait_for(lock, chrono::milliseconds(10));
			}
		}

	private:
		Fraction m_time;
		mutable std::mutex protectTime;
		mutable condition_variable timeChanged;
};

unittest("scheduler: mock clock") {
	std::mutex mutex;
	condition_variable condition;
	Queue<Fraction> q;
	auto f = [&](Fraction time) {
		q.push(time);
		condition.notify_all();
	};

	auto const f1  = Fraction( 1, 1000);
	auto const f10 = Fraction(10, 1000);
	auto clock = shptr(new ClockMock);
	Scheduler s(clock);
	s.scheduleAt(f, f1);
	g_DefaultClock->sleep(f10);
	ASSERT(transferToVector(q).empty());
	clock->setTime(f10);
	{
		unique_lock<std::mutex> lock(mutex);
		auto const durInMs = chrono::milliseconds(100);
		condition.wait_for(lock, durInMs);
	}

	ASSERT_EQUALS(makeVector({f1}), transferToVector(q));
}

template<typename METADATA, typename PORT>
struct DataGenerator : public ModuleS, public virtual IOutputCap {
	DataGenerator() {
		output = addOutput<PORT>();
		output->setMetadata(shptr(new METADATA));
	}
	void process(Data dataIn) override {
		auto data = output->getBuffer(0);
		auto dataPcm = dynamic_pointer_cast<DataPcm>(data);
		if (dataPcm) {
			dataPcm->setPlane(0, nullptr, 1024 * dataPcm->getFormat().getBytesPerSample());
		}
		data->setMediaTime(dataIn->getMediaTime());
		data->setClockTime(dataIn->getClockTime());
		output->emit(data);
	}
	PORT *output;
};

vector<vector<TimePair>> runRectifier(
        Fraction fps,
        shared_ptr<ClockMock> clock,
        const vector<unique_ptr<ModuleS>> &generators,
vector<vector<TimePair>> input) {

	auto rectifier = createModule<TimeRectifier>(1, clock, fps);
	vector<unique_ptr<Utils::Recorder>> recorders;
	for (size_t g = 0; g < generators.size(); ++g) {
		ConnectModules(generators[g].get(), 0, rectifier.get(), g);
		recorders.push_back(create<Utils::Recorder>());
		ConnectModules(rectifier.get(), g, recorders[g].get(), 0);
	}

	for (size_t g = 0; g < generators.size(); ++g) {
		for (auto timePair : input[g]) {
			shared_ptr<DataRaw> data(new DataRaw(0));
			data->setMediaTime(timePair.mediaTime);
			data->setClockTime(timePair.clockTime);
			generators[g]->process(data);
		}
	}
	for (size_t g = 0; g < generators.size(); ++g) {
		for (auto times : input[g]) {
			clock->setTime(Fraction(times.clockTime, IClock::Rate));
		}
	}
	rectifier->flush();

	vector<vector<TimePair>> actualTimes(generators.size());

	for (size_t g = 0; g < generators.size(); ++g) {
		recorders[g]->process(nullptr);
		while (auto data = recorders[g]->pop()) {
			actualTimes[g].push_back(TimePair{data->getMediaTime(), data->getClockTime()});
		}
	}

	clock->setTime(numeric_limits<int32_t>::max());

	return actualTimes;
}


void testRectifierMeta(Fraction fps,
    shared_ptr<ClockMock> clock,
    const vector<unique_ptr<ModuleS>> &generators,
    const vector<vector<TimePair>> &inTimes,
    vector<vector<TimePair>> expectedTimes) {

	auto actualTimes = runRectifier(fps, clock, generators, inTimes);

	for (size_t g = 0; g < generators.size(); ++g) {
		// cut the surplus 'actual' times
		if(actualTimes[g].size() > expectedTimes[g].size())
			actualTimes[g].resize(expectedTimes[g].size());
		else // workaround: don't compare beyond 'actual' times
			expectedTimes[g].resize(actualTimes[g].size());
	}

	ASSERT_EQUALS(expectedTimes, actualTimes);
}

template<typename Metadata, typename PortType>
void testRectifierSinglePort(Fraction fps, const vector<TimePair> &inTimes, const vector<TimePair> &outTimes) {
	vector<vector<TimePair>> in;
	in.push_back(inTimes);
	vector<vector<TimePair>> out;
	out.push_back(outTimes);
	vector<unique_ptr<ModuleS>> generators;
	auto clock = shptr(new ClockMock);
	generators.push_back(createModule<DataGenerator<Metadata, PortType>>(in[0].size(), clock));
	testRectifierMeta(fps, clock, generators, in, out);
}

auto const generateValuesDefault = [](uint64_t step, Fraction fps) {
	auto const t = (int64_t)timescaleToClock(step * fps.den, fps.num);
	return TimePair{t, t};
};

vector<TimePair> generateData(Fraction fps, function<TimePair(uint64_t, Fraction)> generateValue = generateValuesDefault) {
	auto const numItems = (size_t)(Fraction(15) * fps / Fraction(25, 1));
	vector<TimePair> times(numItems);
	for (size_t i = 0; i < numItems; ++i) {
		times[i] = generateValue(i, fps);
	}
	return times;
}

void testFPSFactor(Fraction fps, Fraction factor) {
	auto const genVal = [&](uint64_t step, Fraction fps) {
		auto const tIn = timescaleToClock(step * fps.den, fps.num);
		auto const stepOutIn180k = (IClock::Rate * fps.den * factor.num) / (fps.num * factor.den);
		auto const tOut = tIn / stepOutIn180k * stepOutIn180k;
		return TimePair{(int64_t)tIn, (int64_t)tOut};
	};

	auto const outTimes = generateData(fps * factor, genVal);
	auto const inTimes = generateData(fps);
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps * factor, inTimes, outTimes);
}

unittest("rectifier: FPS factor (single port) 25 fps, x1") {
	testFPSFactor(25, 1);
}

unittest("rectifier: FPS factor (single port) 25 fps, x2") {
	testFPSFactor(25, 2);
}

unittest("rectifier: FPS factor (single port) 25 fps, x1/2") {
	testFPSFactor(25, Fraction(1, 2));
}

unittest("rectifier: FPS factor (single port) 29.97 fps, x1") {
	testFPSFactor(Fraction(30000, 1001), 1);
}

unittest("rectifier: FPS factor (single port) 29.97 fps, x2") {
	testFPSFactor(Fraction(30000, 1001), 2);
}

unittest("rectifier: FPS factor (single port) 29.97 fps, x1/2") {
	testFPSFactor(Fraction(30000, 1001), Fraction(1, 2));
}

unittest("rectifier: initial offset (single port)") {
	auto const fps = Fraction(25, 1);
	auto const inGenVal = [&](uint64_t step, Fraction fps, int shift) {
		auto const t = (int64_t)(IClock::Rate * (step + shift) * fps.den) / fps.num;
		return TimePair{t, int64_t((IClock::Rate * step * fps.den) / fps.num)};
	};

	auto const outTimes = generateData(fps, bind(inGenVal, placeholders::_1, placeholders::_2, 0));
	auto const inTimes1 = generateData(fps, bind(inGenVal, placeholders::_1, placeholders::_2, 5));
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);

	auto const inTimes2 = generateData(fps, bind(inGenVal, placeholders::_1, placeholders::_2, -5));
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);
}

unittest("rectifier: deal with missing frames (single port)") {
	const uint64_t freq = 2;
	auto const fps = Fraction(25, 1);

	auto const inGenVal = [&](uint64_t step, Fraction fps) {
		static uint64_t i = 0;
		if (step && !(step % freq)) i++;
		auto const t = int64_t((IClock::Rate * (step+i) * fps.den) / fps.num);
		return TimePair{t, t};
	};
	auto const inTimes = generateData(fps, inGenVal);

	auto const outGenVal = [&](uint64_t step, Fraction fps) {
		auto const t = (IClock::Rate * step * fps.den) / fps.num;
		static uint64_t prevT = 0, i = 1;
		const uint64_t val = !(i % (freq+1)) ? prevT : t;
		i++; prevT = t;
		return TimePair{int64_t((IClock::Rate * step * fps.den) / fps.num), (int64_t)val};
	};
	auto const outTimes = generateData(fps, outGenVal);

	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes, outTimes);
}

unittest("rectifier: deal with backward discontinuity (single port)") {
	auto const fps = Fraction(25, 1);
	auto const outGenVal = [&](uint64_t step, Fraction fps, int64_t clockTimeOffset, int64_t mediaTimeOffset) {
		auto const mediaTime = (int64_t)(IClock::Rate * (step + mediaTimeOffset) * fps.den) / fps.num;
		auto const clockTime = (int64_t)(IClock::Rate * (step + clockTimeOffset) * fps.den) / fps.num;
		return TimePair{mediaTime, clockTime};
	};
	auto inTimes1 = generateData(fps);
	auto inTimes2 = generateData(fps, bind(outGenVal, placeholders::_1, placeholders::_2, inTimes1.size(), 0));
	auto outTimes1 = generateData(fps);
	auto outTimes2 = generateData(fps, bind(outGenVal, placeholders::_1, placeholders::_2, inTimes1.size(), inTimes1.size()));
	inTimes1.insert(inTimes1.end(), inTimes2.begin(), inTimes2.end());
	outTimes1.insert(outTimes1.end(), outTimes2.begin(), outTimes2.end());
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes1);
}

unittest("rectifier: multiple media types simple") {
	const auto videoRate = Fraction(25, 1);
	const auto audioRate = Fraction(44100, 1024);
	const vector<vector<TimePair>> in = {
		generateData(videoRate),
		generateData(audioRate),
	};
	const vector<vector<TimePair>> out = {
		generateData(videoRate),
		generateData(audioRate),
	};
	vector<unique_ptr<ModuleS>> generators;
	auto clock = shptr(new ClockMock);
	generators.push_back(createModule<DataGenerator<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>>(in[0].size(), clock));
	generators.push_back(createModule<DataGenerator<MetadataRawAudio, OutputPcm>>(in[1].size(), clock));
	testRectifierMeta(videoRate, clock, generators, in, out);
}

unittest("rectifier: fail when no video") {
	ASSERT_THROWN((testRectifierSinglePort<MetadataRawAudio, OutputPcm>(Fraction(25, 1), { { 0, 0 } }, { { 0, 0 } })));
}
