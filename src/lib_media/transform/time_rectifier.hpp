#pragma once

#include "lib_modules/modules.hpp"
#include "lib_utils/scheduler.hpp"
#include <condition_variable>
#include <list>
#include <memory>
#include <vector>

namespace Modules {

static uint64_t const ANALYZE_WINDOW_IN_180K = (5 * IClock::Rate);

/*
This module is responsible for feeding the next modules with a clean signal.

A clean signal has the following properties:
1) its timings are continuous (no gaps, overlaps, or discontinuities - but may not start at zero),
2) the different media are synchronized.

The module needs to be sample accurate (TODO ; ATM we still rely on reframers and act at an AU level). It
operates on raw data. Raw data requires a lot of memory ; however:
1) we store a short duration (typically 500ms) and the framework works by default with pre-allocated pools,
2) RAM is cheap ;)

The module works this way:
 - At each tick it pulls some data (like a mux would).
 - We rely on Clock times. Media times are considered non-reliable and only used to achieve sync.
 - The different media types are processed differently (video = lead, audio = pulled, subtitles = sparse).

Remarks:
 - This module acts as a transframerater for video (by skipping or repeating frames).
 - This module deprecates the AudioConvert class when used as a reframer (i.e. no sample rate conversion).
 - This module feeds compositors or mux with some clean data.
 - TODO (currently handled by demux): This module deprecates heartbeat mechanisms for sparse streams.
*/
class TimeRectifier : public ModuleDynI {
	public:
		TimeRectifier(Fraction frameRate, uint64_t analyzeWindowIn180k = ANALYZE_WINDOW_IN_180K);

		void process() override;
		void flush() override;

		size_t getNumOutputs() const override {
			return outputs.size();
		}
		IOutput* getOutput(size_t i) override {
			mimicOutputs();
			return outputs[i].get();
		}

	private:
		struct Stream {
			std::list<Data> data;
			int64_t numTicks = 0;
			//Data defaultTypeData; //TODO: black screen for video, etc.
		};

		void sanityChecks();
		void mimicOutputs();
		void fillInputQueuesUnsafe();
		void removeOutdatedIndexUnsafe(size_t inputIdx, int64_t removalClockTime);
		void removeOutdatedAllUnsafe(int64_t removalClockTime);
		void declareScheduler(std::unique_ptr<IInput> &input, std::unique_ptr<IOutput> &output);
		void awakeOnFPS(Fraction time);
		Data findNearestData(Stream& stream, Fraction time);
		void findNearestDataAudio(int i, Fraction time, Data& selectedData, Data refData);
		int getMasterStreamId() const;

		std::vector<size_t> getInputs() const {
			std::vector<size_t> r;
			for (size_t i = 0; i < getNumInputs() - 1; ++i)
				r.push_back(i);
			return r;
		}

		Fraction const frameRate;
		int64_t const threshold;
		int64_t analyzeWindowIn180k = 0, maxClockTimeIn180k = 0;
		std::vector<Stream> streams;
		std::mutex inputMutex;
		std::condition_variable flushedCond;
		std::unique_ptr<IScheduler> scheduler;
		bool hasVideo = false, flushing = false;
};

template <>
struct ModuleDefault<TimeRectifier> : public ClockCap, public TimeRectifier {
	template <typename ...Args>
	ModuleDefault(size_t allocatorSize, const std::shared_ptr<IClock> clock, Args&&... args)
		: ClockCap(clock), TimeRectifier(std::forward<Args>(args)...) {
		this->allocatorSize = allocatorSize;
	}
};

}
