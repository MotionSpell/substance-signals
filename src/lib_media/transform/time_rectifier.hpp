#pragma once

#include "lib_modules/modules.hpp"
#include "lib_utils/scheduler.hpp"
#include <list>
#include <memory>
#include <vector>

namespace Modules {

/*This module is responsible for feeding the next modules with a clean signal.

A clean signal has the following properties:
1) it is clean (no gaps, overlaps, or dicontinuity),
2) its timings are continuous,
3) the different media are synchronized.

To achieve these goals:
1) All streams which need to be synchronized need to go through the same TimeRectify module.
2) One needs to TODOXXX update the metadata on the output pins according to the encoder settings TODOXXX should be done automatically?

The module needs to be sample accurate. It operates on raw data. Raw data may take a lot of memory but
1) we store a short duration (typically 500ms),
2) the framework works by default with pre-allocated pools,
3) RAM is cheap ;)

Input data:
- Metadata: we process different media type (audio, video, subtitles) differently.
- Input
- Media time.
 - System time.

Parameters:
- Explicit:
  * The time window expressed as a system time duration.
- Implicit:
  * Output FPS / frame-tick frequency.

Output data:
- Each input has an output with the same data type.
- Specific configuration of the output (such as the framerate for video streams) is inferred from the next module).

Technically:
) This module has a raw buffer duration for each input. Input data older than that (except what is required for one output sample TODO: keep the last input sample) will be discarded.
) Media timestamps are considered non-reliable and only for sync purpose.
x) quieries a clock / ticks other than data => regulator ; technically we could extend it so that output modules pull the data when they needed it.
x) operates as a mux
x) could use metadata to signal problems such as blank frames

This module acts as a transframerater.
This module deprecates the AudioConvert class when used as a reframer.
This module deprecates heartbeat mechanisms for sparse streams.
*/
class TimeRectifier : public ModuleDynI {
public:
	TimeRectifier(uint64_t analyzeWindowIn180k = Clock::Rate / 2, std::unique_ptr<IScheduler> scheduler = uptr(new Scheduler));

	void process() override;
	//Romain: //TODO: flush()

	size_t getNumOutputs() const override {
		return outputs.size();
	}
	IOutput* getOutput(size_t i) override {
		mimicOutputs();
		return outputs[i].get();
	}

private:
#if 0 //Romain
	void addOutputFromType(StreamType type) { //Romain: unused?
		switch (type) {
		case VIDEO_RAW:    addOutput<OutputPicture>(); break;
		case AUDIO_RAW:    addOutput<OutputPcm>();     break;
		case SUBTITLE_PKT: addOutput<OutputPcm>();     break;
		default: throw error("unhandled media type (mimicInputToOutput)");
		}
	}
#endif

	void mimicOutputs();
	void fillInputQueues();
	void removeOutdated();
	void awakeOnFPS();

	struct Stream {
		std::list<Data> data;
		//Data defaultTypeData; //Romain: black screen for video, etc.
	};

	uint64_t analyzeWindowIn180k;
	std::vector<std::unique_ptr<Stream>> input;
	std::unique_ptr<IScheduler> scheduler;
	bool hasVideo = false;
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
