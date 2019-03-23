#include "tests/tests.hpp"
#include "lib_utils/i_scheduler.hpp"
#include "lib_media/transform/time_rectifier.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_media/common/pcm.hpp"
#include "lib_media/common/picture.hpp"
#include "lib_media/common/metadata.hpp"

#include <algorithm> // sort
#include <cassert>

using namespace std;
using namespace Tests;
using namespace Modules;

struct Event {
	int index;
	int64_t clockTime;
	int64_t mediaTime;
	bool operator<(Event other) const {
		if(clockTime != other.clockTime)
			return clockTime < other.clockTime;
		if(index != other.index)
			return index < other.index;
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

		Id scheduleAt(TaskFunc &&task, Fraction time) override {
			if(time < m_time)
				throw runtime_error("The rectifier is scheduling events in the past");

			m_tasks.push_back({time, task});
			std::sort(m_tasks.begin(), m_tasks.end());
			return -1;
		}

		Id scheduleIn(TaskFunc &&, Fraction) override {
			assert(0);
			return -1;
		}

		void cancel(Id id) override {
			(void)id;
			m_tasks.pop_back();
		}

		int getPendingTaskCount() const {
			return (int)m_tasks.size();
		}

	private:
		Fraction m_time = 0;

		struct Task {
			Fraction time;
			TaskFunc func;
			bool operator<(Task const& other) const {
				return time < other.time;
			}
		};

		vector<Task> m_tasks; // keep this sorted
};

template<typename METADATA, typename TYPE>
struct DataGenerator : public ModuleS, public virtual IOutputCap {
	DataGenerator() {
		output = addOutput<OutputDefault>();
		output->setMetadata(make_shared<METADATA>());
	}
	void processOne(Data dataIn) override {
		auto data = output->template getBuffer<TYPE>(0);
		auto dataPcm = dynamic_pointer_cast<DataPcm>(data);
		if (dataPcm) {
			dataPcm->setPlane(0, nullptr, 1024 * dataPcm->getFormat().getBytesPerSample());
		}
		data->setMediaTime(dataIn->getMediaTime());
		output->post(data);
	}
	OutputDefault *output;
};

struct DataRecorder : ModuleS {
	DataRecorder(shared_ptr<IClock> clock_) : clock(clock_) {
	}

	void processOne(Data data) {
		if(!data)
			return;
		auto now = fractionToClock(clock->now());
		record.push_back({now, data});
	}

	struct Rec {
		int64_t when;
		Data data;
	};

	vector<Rec> record;
	shared_ptr<IClock> clock;
};

typedef DataGenerator<MetadataRawVideo, DataPicture> VideoGenerator;
typedef DataGenerator<MetadataRawAudio, DataPcm> AudioGenerator;

vector<Event> runRectifier(
    Fraction fps,
    shared_ptr<ClockMock> clock,
    const vector<unique_ptr<ModuleS>> &generators,
    vector<Event> events) {

	const int N = (int)generators.size();

	auto rectifier = createModuleWithSize<TimeRectifier>(1, &NullHost, clock, clock.get(), fps);
	vector<unique_ptr<DataRecorder>> recorders;
	for (int i = 0; i < N; ++i) {
		ConnectModules(generators[i].get(), 0, rectifier.get(), i);
		recorders.push_back(createModule<DataRecorder>(clock));
		ConnectModules(rectifier.get(), i, recorders[i].get(), 0);
	}

	for (auto event : events) {
		if(event.clockTime > 0)
			clock->setTime(Fraction(event.clockTime, IClock::Rate));
		shared_ptr<DataRaw> data(new DataRaw(0));
		data->setMediaTime(event.mediaTime);
		generators[event.index]->processOne(data);
	}

	for(int i=0; i < 100; ++i)
		clock->setTime(clock->now());

	vector<Event> actualTimes;

	for(int i=0; i < N; ++i) {
		recorders[i]->processOne(nullptr);
		for (auto& rec : recorders[i]->record) {
			actualTimes.push_back(Event{i, rec.when, rec.data->getMediaTime()});
		}
	}
	sort(actualTimes.begin(), actualTimes.end());

	return actualTimes;
}

unittest("rectifier: simple offset") {
	// use '1000' as a human-readable frame period
	auto fps = Fraction(IClock::Rate, 1000);

	auto const inTimes = vector<Event>({
		Event{0, 8801000, 301007},
		Event{0, 8802000, 301007},
		Event{0, 8803000, 302007},
		Event{0, 8804000, 303007},
		Event{0, 8805000, 304007},
	});
	auto const expectedTimes = vector<Event>({
		Event{0, 8801000, 0},
		Event{0, 8802000, 1000},
		Event{0, 8803000, 2000},
		Event{0, 8804000, 3000},
		Event{0, 8805000, 4000},
	});

	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModuleWithSize<VideoGenerator>(inTimes.size()));

	ASSERT_EQUALS(expectedTimes, runRectifier(fps, clock, generators, inTimes));
	ASSERT_EQUALS(0, clock->getPendingTaskCount());
}

unittest("rectifier: missing frame") {
	// use '100' as a human-readable frame period
	auto fps = Fraction(IClock::Rate, 100);

	auto const inTimes = vector<Event>({
		Event{0, 0, 30107},
		Event{0, 100, 30107},
		// missing Event{0, 2000, 302007},
		Event{0, 300, 30307},
		Event{0, 400, 30407},
		Event{0, 500, 30507},
	});
	auto const expectedTimes = vector<Event>({
		Event{0, 0, 0},
		Event{0, 100, 100},
		Event{0, 200, 200},
		Event{0, 300, 300},
		Event{0, 400, 400},
		Event{0, 500, 500},
	});

	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModuleWithSize<VideoGenerator>(inTimes.size()));

	ASSERT_EQUALS(expectedTimes, runRectifier(fps, clock, generators, inTimes));
}

unittest("rectifier: noisy timestamps") {
	// use '100' as a human-readable frame period
	auto fps = Fraction(IClock::Rate, 100);

	auto const inTimes = vector<Event>({
		Event{0,   0 - 4, 1000 + 2},
		Event{0, 100 + 5, 1100 - 3},
		Event{0, 200 - 1, 1200 - 1},
		Event{0, 300 + 2, 1300 + 7},
		Event{0, 400 - 2, 1400 - 9},
		Event{0, 500 + 1, 1500 + 15},
	});
	auto const expectedTimes = vector<Event>({
		Event{0,   0,   0},
		Event{0, 100, 100},
		Event{0, 200, 200},
		Event{0, 300, 300},
		Event{0, 400, 400},
		Event{0, 500, 500},
	});

	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModuleWithSize<VideoGenerator>(inTimes.size()));

	ASSERT_EQUALS(expectedTimes, runRectifier(fps, clock, generators, inTimes));
}

static void fixupTimes(vector<Event>& expectedTimes, vector<Event>& actualTimes) {
	// cut the surplus 'actual' times
	if(actualTimes.size() > expectedTimes.size())
		actualTimes.resize(expectedTimes.size());
	else if(expectedTimes.size() - actualTimes.size() <= 3)
		// workaround: don't compare beyond 'actual' times
		expectedTimes.resize(actualTimes.size());
}

template<typename GeneratorType>
void testRectifierSinglePort(Fraction fps, vector<Event> inTimes, vector<Event> expectedTimes) {
	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModuleWithSize<GeneratorType>(inTimes.size()));

	auto actualTimes = runRectifier(fps, clock, generators, inTimes);

	fixupTimes(expectedTimes, actualTimes);

	ASSERT_EQUALS(expectedTimes, actualTimes);
}

struct TimePair {
	int64_t mediaTime;
	int64_t clockTime;
};

TimePair generateValuesDefault(uint64_t step, Fraction fps) {
	auto const t = (int64_t)fractionToClock(fps.inverse() * step);
	return TimePair{t, t};
};

vector<Event> generateData(Fraction fps, function<TimePair(uint64_t, Fraction)> generateValue = generateValuesDefault) {
	auto const numItems = (size_t)(Fraction(15) * fps / Fraction(25, 1));
	vector<Event> times(numItems);
	for (size_t i = 0; i < numItems; ++i) {
		auto tp = generateValue(i, fps);
		times[i] = Event{0, tp.clockTime, tp.mediaTime};
	}
	return times;
}

void testFPSFactor(Fraction fps, Fraction factor) {
	auto const genVal = [](uint64_t step, Fraction fps) {
		auto const tIn = timescaleToClock(step * fps.den, fps.num);
		return TimePair{(int64_t)tIn, (int64_t)tIn};
	};

	auto const outTimes = generateData(fps * factor, genVal);
	auto const inTimes = generateData(fps);
	testRectifierSinglePort<VideoGenerator>(fps * factor, inTimes, outTimes);
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
	auto const fps = Fraction(25, 1);
	auto const inGenVal = [](uint64_t step, Fraction fps, int shift) {
		auto const t = (int64_t)(IClock::Rate * (step + shift) * fps.den) / fps.num;
		return TimePair{t, int64_t((IClock::Rate * step * fps.den) / fps.num)};
	};

	auto const outTimes = generateData(fps, bind(inGenVal, placeholders::_1, placeholders::_2, 0));
	auto const inTimes1 = generateData(fps, bind(inGenVal, placeholders::_1, placeholders::_2, 5));
	testRectifierSinglePort<VideoGenerator>(fps, inTimes1, outTimes);

	auto const inTimes2 = generateData(fps, bind(inGenVal, placeholders::_1, placeholders::_2, -5));
	testRectifierSinglePort<VideoGenerator>(fps, inTimes1, outTimes);
}

unittest("rectifier: deal with missing frames (single port)") {
	const uint64_t freq = 2;
	auto const fps = Fraction(25, 1);

	auto const inGenVal = [=](uint64_t step, Fraction fps) {
		static uint64_t i = 0;
		if (step && !(step % freq)) i++;
		auto const t = int64_t((IClock::Rate * (step+i) * fps.den) / fps.num);
		return TimePair{t, t};
	};
	auto const inTimes = generateData(fps, inGenVal);

	auto const outGenVal = [](uint64_t step, Fraction fps) {
		auto const t = int64_t((IClock::Rate * step * fps.den) / fps.num);
		return TimePair{t, t};
	};
	auto const outTimes = generateData(fps, outGenVal);

	testRectifierSinglePort<VideoGenerator>(fps, inTimes, outTimes);
}

unittest("rectifier: deal with backward discontinuity (single port)") {
	auto const fps = Fraction(25, 1);
	auto const outGenVal = [](uint64_t step, Fraction fps, int64_t clockTimeOffset, int64_t mediaTimeOffset) {
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
	testRectifierSinglePort<VideoGenerator>(fps, inTimes1, outTimes1);
}

unittest("rectifier: multiple media types simple") {

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
	generators.push_back(createModuleWithSize<VideoGenerator>(times.size()));
	generators.push_back(createModuleWithSize<AudioGenerator>(times.size()));

	auto actualTimes = runRectifier(videoRate, clock, generators, times);

	// Quantize output delivery times to the internal media period
	// of the rectifier. We don't expect it to wake up more often than that.
	{
		auto const outputPeriod = fractionToClock(videoRate.inverse());
		for(auto& event : times)
			event.clockTime = int64_t((event.clockTime + outputPeriod - 1) / outputPeriod) * outputPeriod;
		sort(times.begin(), times.end());
	}

	ASSERT_EQUALS(times, actualTimes);
}

unittest("rectifier: two streams, only the first receives data") {
	const auto videoRate = Fraction(25, 1);
	auto times = generateData(videoRate);
	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModuleWithSize<VideoGenerator>(100));
	generators.push_back(createModuleWithSize<AudioGenerator>(100));

	auto actualTimes = runRectifier(videoRate, clock, generators, times);

	ASSERT_EQUALS(times, actualTimes);
}

unittest("rectifier: fail when no video") {
	vector<unique_ptr<ModuleS>> generators;
	auto clock = make_shared<ClockMock>();
	generators.push_back(createModuleWithSize<AudioGenerator>(1));

	ASSERT_THROWN(runRectifier(Fraction(25, 1), clock, generators, {Event()}));
}
