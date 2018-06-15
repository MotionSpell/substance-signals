#include "tests/tests.hpp"
#include "lib_utils/queue_inspect.hpp"
#include "lib_utils/i_scheduler.hpp"
#include "lib_media/transform/time_rectifier.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_media/common/pcm.hpp"
#include "lib_media/common/picture.hpp"
#include "lib_media/common/metadata.hpp"

using namespace std;
using namespace Tests;
using namespace Modules;

struct TimePair {
	int64_t mediaTime;
	int64_t clockTime;
};

struct Event {
	int index;
	int64_t clockTime;
	int64_t mediaTime;
	bool operator<(Event other) const {
		if(clockTime != other.clockTime)
			return clockTime < other.clockTime;
		else
			return mediaTime < other.mediaTime;
	}
};

// allows ASSERT_EQUALS on Event
static std::ostream& operator<<(std::ostream& o, Event t) {
	o << "{ #" << t.index << " " << t.mediaTime << "-" << t.clockTime << "}";
	return o;
}

static bool operator==(Event a, Event b) {
	return a.index == b.index && a.clockTime == b.clockTime && a.mediaTime == b.mediaTime;
}

class ClockMock : public IClock, public IScheduler {
	public:
		void setTime(Fraction t) {
			assert(t >= m_time);

			// beware: running tasks might modify m_tasks by pushing new tasks
			while(!m_tasks.empty() && m_tasks[0].time <= m_time) {
				m_time = m_tasks[0].time;
				m_tasks[0].func(m_time);
				m_tasks.erase(m_tasks.begin());
			}

			m_time = t;
		}

		Fraction now() const override {
			return m_time;
		}

		void scheduleAt(TaskFunc &&task, Fraction time) {
			assert(time >= m_time);
			m_tasks.push_back({time, task});
			std::sort(m_tasks.begin(), m_tasks.end());
		}

		double getSpeed() const override {
			return 0.0;
		}

		void sleep(Fraction) const override {
			assert(0);
		}

		void scheduleIn(TaskFunc &&, Fraction) {
			assert(0);
		}

	private:
		Fraction m_time = Fraction(-1, 1000);

		struct Task {
			Fraction time;
			TaskFunc func;
			bool operator<(Task const& other) {
				return time < other.time;
			}
		};

		vector<Task> m_tasks; // keep this sorted
};

template<typename METADATA, typename PORT>
struct DataGenerator : public ModuleS, public virtual IOutputCap {
	DataGenerator() {
		output = addOutput<PORT>();
		output->setMetadata(make_shared<METADATA>());
	}
	void process(Data dataIn) override {
		auto data = output->getBuffer(0);
		auto dataPcm = dynamic_pointer_cast<DataPcm>(data);
		if (dataPcm) {
			dataPcm->setPlane(0, nullptr, 1024 * dataPcm->getFormat().getBytesPerSample());
		}
		data->setMediaTime(dataIn->getMediaTime());
		data->setCreationTime(dataIn->getCreationTime());
		output->emit(data);
	}
	PORT *output;
};

vector<Event> mergeEvents(vector<vector<TimePair>> input) {
	std::vector<Event> r;

	for (int i = 0; i < (int)input.size(); ++i) {
		for (auto times : input[i]) {
			r.push_back(Event{i, times.clockTime, times.mediaTime});
		}
	}

	std::sort(r.begin(), r.end());
	return r;
}

vector<Event> runRectifier(
    Fraction fps,
    shared_ptr<ClockMock> clock,
    const vector<unique_ptr<ModuleS>> &generators,
    vector<Event> events) {

	const int N = (int)generators.size();

	auto scheduler = clock.get();
	auto rectifier = createModule<TimeRectifier>(1, clock, scheduler, fps);
	vector<unique_ptr<Utils::Recorder>> recorders;
	for (int i = 0; i < N; ++i) {
		ConnectModules(generators[i].get(), 0, rectifier.get(), i);
		recorders.push_back(create<Utils::Recorder>());
		ConnectModules(rectifier.get(), i, recorders[i].get(), 0);
	}

	for (auto event : events) {
		shared_ptr<DataRaw> data(new DataRaw(0));
		data->setMediaTime(event.mediaTime);
		data->setCreationTime(event.clockTime);
		generators[event.index]->process(data);
		if(event.clockTime > 0)
			clock->setTime(Fraction(event.clockTime, IClock::Rate));
	}

	{
		std::thread flushThread([&]() {
			rectifier->flush();
		});
		std::this_thread::sleep_for(10ms);
		for(int i=1; i < 100; ++i)
			clock->setTime(clock->now() + 1);
		flushThread.join();
	}

	vector<Event> actualTimes;

	for(int i=0; i < N; ++i) {
		recorders[i]->process(nullptr);
		while (auto data = recorders[i]->pop()) {
			actualTimes.push_back(Event{i, data->getCreationTime(), data->getMediaTime()});
		}
	}
	sort(actualTimes.begin(), actualTimes.end());

	return actualTimes;
}

static void fixupTimes(vector<Event>& expectedTimes, vector<Event>& actualTimes) {
	// cut the surplus 'actual' times
	if(actualTimes.size() > expectedTimes.size())
		actualTimes.resize(expectedTimes.size());
	else if(expectedTimes.size() - actualTimes.size() <= 3)
		// workaround: don't compare beyond 'actual' times
		expectedTimes.resize(actualTimes.size());
}

template<typename Metadata, typename PortType>
void testRectifierSinglePort(Fraction fps, vector<Event> inTimes, vector<Event> expectedTimes) {
	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModule<DataGenerator<Metadata, PortType>>(inTimes.size(), clock));

	auto actualTimes = runRectifier(fps, clock, generators, inTimes);

	fixupTimes(expectedTimes, actualTimes);

	ASSERT_EQUALS(expectedTimes, actualTimes);

}

auto const generateValuesDefault = [](uint64_t step, Fraction fps) {
	auto const t = (int64_t)timescaleToClock(step * fps.den, fps.num);
	return TimePair{t, t};
};

vector<Event> generateData(Fraction fps, function<TimePair(uint64_t, Fraction)> generateValue = generateValuesDefault) {
	auto const numItems = (size_t)(Fraction(15) * fps / Fraction(25, 1));
	vector<TimePair> times(numItems);
	for (size_t i = 0; i < numItems; ++i) {
		times[i] = generateValue(i, fps);
	}
	return mergeEvents({times});
}

void testFPSFactor(Fraction fps, Fraction factor) {
	ScopedLogLevel lev(Quiet);
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

unittest("rectifier: FPS factor (single port) 9 fps, x1") {
	testFPSFactor(9, 1);
}

unittest("rectifier: FPS factor (single port) 9 fps, x2") {
	testFPSFactor(9, 2);
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
	ScopedLogLevel lev(Quiet);
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
	ScopedLogLevel lev(Quiet);
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
	ScopedLogLevel lev(Quiet);
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
	ScopedLogLevel lev(Quiet);

	vector<Event> times;

	const auto videoRate = Fraction(25, 1);
	for(auto ev : generateData(videoRate)) {
		ev.index = 0;
		times.push_back(ev);
	}

	const auto audioRate = Fraction(44100, 1024);
	for(auto ev : generateData(audioRate)) {
		ev.index = 1;
		times.push_back(ev);
	}

	sort(times.begin(), times.end());

	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModule<DataGenerator<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>>(times.size(), clock));
	generators.push_back(createModule<DataGenerator<MetadataRawAudio, OutputPcm>>(times.size(), clock));

	auto actualTimes = runRectifier(videoRate, clock, generators, times);

	auto expectedTimes = times;
	fixupTimes(expectedTimes, actualTimes);

	ASSERT_EQUALS(expectedTimes, actualTimes);
}

unittest("rectifier: two streams, only the first receives data") {
	ScopedLogLevel lev(Quiet);
	const auto videoRate = Fraction(25, 1);
	auto times = generateData(videoRate);
	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModule<DataGenerator<MetadataRawVideo, OutputDataDefault<PictureYUV420P>>>(100, clock));
	generators.push_back(createModule<DataGenerator<MetadataRawAudio, OutputPcm>>(100, clock));

	auto actualTimes = runRectifier(videoRate, clock, generators, times);

	auto expectedTimes = times;
	fixupTimes(expectedTimes, actualTimes);
	ASSERT_EQUALS(expectedTimes, actualTimes);
}

unittest("rectifier: fail when no video") {
	ScopedLogLevel lev(Quiet);
	ASSERT_THROWN((testRectifierSinglePort<MetadataRawAudio, OutputPcm>(Fraction(25, 1), {}, {})));
}
