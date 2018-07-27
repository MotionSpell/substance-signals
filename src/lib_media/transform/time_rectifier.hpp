#pragma once

#include "lib_modules/modules.hpp"
#include "lib_modules/core/log.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_utils/i_scheduler.hpp"
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
class TimeRectifier : public ModuleDynI, private LogCap {
	public:
		TimeRectifier(std::shared_ptr<IClock> clock_, IScheduler* scheduler, Fraction frameRate);

		void process() override;

		int getNumOutputs() const override {
			return (int)outputs.size();
		}
		IOutput* getOutput(int i) override {
			mimicOutputs();
			return outputs[i].get();
		}

	private:
		struct Stream {
			struct Rec {
				int64_t creationTime;
				Data data;
			};

			std::vector<Rec> data;
			int64_t numTicks = 0;
		};

		void sanityChecks();
		void mimicOutputs();
		void fillInputQueuesUnsafe();
		void discardStreamOutdatedData(size_t inputIdx, int64_t removalClockTime);
		void discardOutdatedData(int64_t removalClockTime);
		void declareScheduler(std::unique_ptr<IInput> &input, std::unique_ptr<IOutput> &output);
		void emitOnePeriod(Fraction time);
		Data findNearestData(Stream& stream, Fraction time);
		void findNearestDataAudio(size_t i, Fraction time, Data& selectedData, int64_t masterTime);
		size_t getMasterStreamId() const;

		Fraction const frameRate;
		int64_t const threshold;
		std::vector<Stream> streams;
		std::mutex inputMutex;
		std::shared_ptr<IClock> clock;
		IScheduler* const scheduler;
		bool hasVideo = false;
};

template <>
struct ModuleDefault<TimeRectifier> : public TimeRectifier {
	template <typename ...Args>
	ModuleDefault(size_t allocatorSize, Args&&... args)
		: TimeRectifier(std::forward<Args>(args)...) {
		this->allocatorSize = allocatorSize;
	}
};

}
