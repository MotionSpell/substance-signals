#include "tests.hpp"
#include "lib_media/transform/time_rectifier.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_media/common/pcm.hpp"
#include "lib_media/common/picture.hpp"
#include <cmath>
#include <iostream>

using namespace Tests;
using namespace Modules;

class ClockMock : public IClock {
	public:
		ClockMock(const Fraction &time = Fraction(-1, 1000)) : time(time) {}
		virtual ~ClockMock() {
			condition.notify_all();
		}
		void setTime(const Fraction &t) {
			if (t > time) {
				time = t;
			}
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
		Fraction time;
		mutable std::mutex mutex;
		mutable std::condition_variable condition;
};

unittest("scheduler: mock clock") {
	std::mutex mutex;
	std::condition_variable condition;
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
	ASSERT(q.size() == 0);
	clock->setTime(f10);
	{
		std::unique_lock<std::mutex> lock(mutex);
		auto const durInMs = std::chrono::milliseconds(100);
		condition.wait_for(lock, durInMs);
	}
	ASSERT(q.size() == 1);
	auto const t = q.pop();
	ASSERT(t == f1);
}

template<typename METADATA, typename PORT>
struct DataGenerator : public ModuleS, public virtual IOutputCap {
	DataGenerator() {
		output = addOutput<PORT>();
		output->setMetadata(shptr(new METADATA));
	}
	void process(Data dataIn) override {
		auto data = output->getBuffer(0);
		auto dataPcm = std::dynamic_pointer_cast<DataPcm>(data);
		if (dataPcm) {
			dataPcm->setPlane(0, nullptr, 1024 * dataPcm->getFormat().getBytesPerSample());
		}
		data->setMediaTime(dataIn->getMediaTime());
		data->setClockTime(dataIn->getClockTime());
		output->emit(data);
	}
	PORT *output;
};

void testRectifierMeta(const Fraction &fps, std::shared_ptr<ClockMock> clock,
    const std::vector<std::unique_ptr<ModuleS>> &generators,
    const std::vector<std::vector<std::pair<int64_t, int64_t>>> &inTimes, const std::vector<std::vector<std::pair<int64_t, int64_t>>> &outTimes) {
	auto rectifier = createModule<TimeRectifier>(1, clock, fps);
	std::vector<std::unique_ptr<Utils::Recorder>> recorders;
	for (size_t g = 0; g < generators.size(); ++g) {
		ConnectModules(generators[g].get(), 0, rectifier.get(), g);
		recorders.push_back(create<Utils::Recorder>());
		ConnectModules(rectifier.get(), g, recorders[g].get(), 0);
	}

	std::shared_ptr<DataRaw> data(new DataRaw(0));
	for (size_t g = 0; g < generators.size(); ++g) {
		for (size_t i = 0; i < inTimes[g].size(); ++i) {
			data->setMediaTime(inTimes[g][i].first);
			data->setClockTime(inTimes[g][i].second);
			generators[g]->process(data);
		}
	}
	for (size_t g = 0; g < generators.size(); ++g) {
		for (size_t i = 0; i < inTimes[g].size(); ++i) {
			clock->setTime(Fraction(inTimes[g][i].second, Clock::Rate));
		}
	}
	rectifier->flush();

	for (size_t g = 0; g < generators.size(); ++g) {
		recorders[g]->process(nullptr);
		size_t i = 0, iMax = std::min<size_t>(inTimes[g].size(), outTimes[g].size());
		Data data;
		while ((data = recorders[g]->pop()) && (i < iMax)) {
			Log::msg(Debug, "recv[%s] %s-%s (expected %s-%s)", g, data->getMediaTime(), data->getClockTime(), outTimes[g][i].first, outTimes[g][i].second);
			ASSERT(data->getMediaTime() == outTimes[g][i].first);
			ASSERT(data->getClockTime() == outTimes[g][i].second);
			i++;
		}
		ASSERT(i >= iMax - 2);
	}
	clock->setTime(std::numeric_limits<int32_t>::max());
}

template<typename Metadata, typename PortType>
void testRectifierSinglePort(const Fraction &fps, const std::vector<std::pair<int64_t, int64_t>> &inTimes, const std::vector<std::pair<int64_t, int64_t>> &outTimes) {
	std::vector<std::vector<std::pair<int64_t, int64_t>>> in;
	in.push_back(inTimes);
	std::vector<std::vector<std::pair<int64_t, int64_t>>> out;
	out.push_back(outTimes);
	std::vector<std::unique_ptr<ModuleS>> generators;
	auto clock = shptr(new ClockMock);
	generators.push_back(createModule<DataGenerator<Metadata, PortType>>(in[0].size(), clock));
	testRectifierMeta(fps, clock, generators, in, out);
}

auto const generateValuesDefault = [](uint64_t step, Fraction fps) {
	auto const t = timescaleToClock(step * fps.den, fps.num);
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
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps * factor, inTimes, outTimes);
}

unittest("rectifier: FPS factor (single port)") {
	auto const FPSs = { Fraction(25, 1), Fraction(30000, 1001) };
	auto const factors = { Fraction(1, 1), Fraction(2, 1), Fraction(1, 2) };
	for (auto &fps : FPSs) {
		for (auto &factor : factors) {
			Log::msg(Info, "Testing FPS %s/%s with output factor %s/%s", fps.num, fps.den, factor.num, factor.den);
			testFPSFactor(fps, factor);
		}
	}
}

unittest("rectifier: initial offset (single port)") {
	auto const fps = Fraction(25, 1);
	auto const inGenVal = [&](uint64_t step, Fraction fps, int shift) {
		auto const t = (int64_t)(Clock::Rate * (step + shift) * fps.den) / fps.num;
		return std::pair<int64_t, int64_t >(t, (Clock::Rate * step * fps.den) / fps.num);
	};

	auto const outTimes = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2, 0));
	auto const inTimes1 = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2, 5));
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);

	auto const inTimes2 = generateData(fps, std::bind(inGenVal, std::placeholders::_1, std::placeholders::_2, -5));
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes);
}

unittest("rectifier: deal with missing frames (single port)") {
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

	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes, outTimes);
}

unittest("rectifier: deal with backward discontinuity (single port)") {
	auto const fps = Fraction(25, 1);
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
	testRectifierSinglePort<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>(fps, inTimes1, outTimes1);
}

unittest("rectifier: multiple media types simple") {
	const Fraction fps1 = 25, fps2 = Fraction(44100, 1024);
	std::vector<std::vector<std::pair<int64_t, int64_t>>> in;
	in.push_back(generateData(fps1)); //simulate video
	in.push_back(generateData(fps2)); //simulate audio
	std::vector<std::vector<std::pair<int64_t, int64_t>>> out;
	out.push_back(generateData(fps1));
	out.push_back(generateData(fps2));
	std::vector<std::unique_ptr<ModuleS>> generators;
	auto clock = shptr(new ClockMock);
	generators.push_back(createModule<DataGenerator<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>>(in[0].size(), clock));
	generators.push_back(createModule<DataGenerator<MetadataRawAudio, OutputPcm>>(in[1].size(), clock));
	testRectifierMeta(fps1, clock, generators, in, out);
}

unittest("rectifier: fail when no video") {
	bool thrown = false;
	try {
		testRectifierSinglePort<MetadataRawAudio, OutputPcm>(Fraction(25, 1), { { 0, 0 } }, { { 0, 0 } });
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}
