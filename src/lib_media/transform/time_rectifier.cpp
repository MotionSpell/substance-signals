#include "time_rectifier.hpp"
#include "lib_utils/scheduler.hpp"
#include "../common/pcm.hpp"

static auto const analyzeWindowIn180k = IClock::Rate / 2; // 500 ms

namespace Modules {

#define TR_DEBUG Debug

TimeRectifier::TimeRectifier(IScheduler* scheduler_, Fraction frameRate)
	: frameRate(frameRate),
	  threshold(timescaleToClock(frameRate.den, frameRate.num)),
	  scheduler(scheduler_) {
}

void TimeRectifier::sanityChecks() {
	if (!hasVideo)
		throw error("requires to have one video stream connected");
}

void TimeRectifier::process() {
	std::unique_lock<std::mutex> lock(inputMutex);
	fillInputQueuesUnsafe();
	sanityChecks();
	discardOutdatedData((fractionToClock(clock->now()) - analyzeWindowIn180k));
}

void TimeRectifier::mimicOutputs() {
	while(streams.size() < getInputs().size()) {
		std::unique_lock<std::mutex> lock(inputMutex);
		addOutput<OutputDefault>();
		streams.push_back(Stream());
	}
}

void TimeRectifier::declareScheduler(std::unique_ptr<IInput> &input, std::unique_ptr<IOutput> &output) {
	auto const oMeta = output->getMetadata();
	if (!oMeta) {
		log(Debug, "Output isn't connected or doesn't expose a metadata: impossible to check.");
	} else if (input->getMetadata()->getStreamType() != oMeta->getStreamType())
		throw error("Metadata I/O inconsistency");

	if (input->getMetadata()->getStreamType() == VIDEO_RAW) {
		if (hasVideo)
			throw error("Only one video stream is allowed");
		hasVideo = true;
		scheduleEvery(scheduler, std::bind(&TimeRectifier::emitOnePeriod, this, std::placeholders::_1), frameRate.inverse(), clock->now());
	}
}

void TimeRectifier::fillInputQueuesUnsafe() {
	auto now = fractionToClock(clock->now());
	maxClockTimeIn180k = std::max(maxClockTimeIn180k, now);

	for (auto i : getInputs()) {
		auto &currInput = inputs[i];
		Data data;
		while (currInput->tryPop(data)) {
			maxClockTimeIn180k = std::max(maxClockTimeIn180k, now);
			streams[i].data.push_back({now, data});
			if (currInput->updateMetadata(data)) {
				declareScheduler(currInput, outputs[i]);
			}
		}
	}
}

void TimeRectifier::discardOutdatedData(int64_t removalClockTime) {
	for (auto i : getInputs()) {
		discardStreamOutdatedData(i, removalClockTime);
	}
}

void TimeRectifier::discardStreamOutdatedData(size_t inputIdx, int64_t removalClockTime) {
	auto minQueueSize = 1;
	auto& stream = streams[inputIdx];
	auto data = stream.data.begin();
	while ((int)stream.data.size() > minQueueSize && data != stream.data.end()) {
		if ((*data).creationTime < removalClockTime) {
			log(TR_DEBUG, "Remove last streams[%s] data time media=%s clock=%s (removalClockTime=%s)", inputIdx, (*data).data->getMediaTime(), (*data).creationTime, removalClockTime);
			data = stream.data.erase(data);
		} else {
			data++;
		}
	}
}

Data TimeRectifier::findNearestData(Stream& stream, Fraction time) {
	Data refData;
	auto distClock = std::numeric_limits<int64_t>::max();
	int currDataIdx = -1, idx = -1;
	for (auto &currData : stream.data) {
		idx++;
		auto const currDistClock = currData.creationTime - fractionToClock(time);
		log(Debug, "Video: considering data (%s/%s) at time %s (currDist=%s, dist=%s, threshold=%s)", currData.data->getMediaTime(), currData.creationTime, fractionToClock(time), currDistClock, distClock, threshold);
		if (std::abs(currDistClock) < distClock) {
			/*timings are monotonic so check for a previous data with distance less than one frame*/
			if (currDistClock <= 0 || (currDistClock > 0 && distClock > threshold)) {
				distClock = std::abs(currDistClock);
				refData = currData.data;
				currDataIdx = idx;
			}
		}
	}
	if ((stream.numTicks > 0) && (stream.data.size() >= 2) && (currDataIdx != 1)) {
		log(Debug, "Selected reference data is not contiguous to the last one (index=%s).", currDataIdx);
		//TODO: pass in error mode: flush all the data where the clock time removeOutdatedAllUnsafe(refData->getCreationTime());
	}
	return refData;
}

void TimeRectifier::findNearestDataAudio(size_t i, Fraction time, Data& selectedData, int64_t masterTime) {
	int currDataIdx = -1, idx = -1;
	for (auto &currData : streams[i].data) {
		idx++;
		if (selectedData && !idx) { /*first data cannot be selected*/
			selectedData = nullptr;
			continue;
		}
		auto const currDistMedia = masterTime - currData.data->getMediaTime();
		log(Debug, "Other: considering data (%s/%s) at time %s (ref=%s, currDist=%s)", currData.data->getMediaTime(), currData.creationTime, fractionToClock(time), masterTime, currDistMedia);
		if ((currDistMedia >= 0) && (currDistMedia < threshold)) {
			selectedData = currData.data;
			currDataIdx = idx;
			break;
		}
	}
	if ((streams[i].numTicks > 0) && (streams[i].data.size() >= 2) && (currDataIdx != 1)) {
		log(Warning, "[%s] Selected data is not contiguous to the last one (index=%s). Expect discontinuity in the signal.", i, currDataIdx);
	}
}

size_t TimeRectifier::getMasterStreamId() const {
	for(auto i : getInputs()) {
		if (inputs[i]->getMetadata()->getStreamType() == VIDEO_RAW) {
			return i;
		}
	}
	return 0;
}

// emit one "media period" on every output
void TimeRectifier::emitOnePeriod(Fraction time) {
	std::unique_lock<std::mutex> lock(inputMutex);
	discardOutdatedData(fractionToClock(time) - analyzeWindowIn180k);

	// media time corresponding to the start of the "media period"
	int64_t masterTime = 0;

	{
		auto const i = getMasterStreamId();
		auto& master = streams[i];
		auto refData = findNearestData(master, time);
		if (!refData) {
			assert(master.numTicks == 0);

			log(Warning, "No available reference data for clock time %s", fractionToClock(time));
			return;
		}

		masterTime = refData->getMediaTime();

		if (master.numTicks == 0) {
			log(Info, "First available reference clock time: %s", fractionToClock(time));
		}

		auto data = make_shared<DataBaseRef>(refData);
		data->setMediaTime(fractionToClock(Fraction(master.numTicks++ * frameRate.den, frameRate.num)));
		log(TR_DEBUG, "Video: send[%s:%s] t=%s (data=%s) (ref %s)", i, master.data.size(), data->getMediaTime(), data->getMediaTime(), refData->getMediaTime());
		outputs[i]->emit(data);
		discardStreamOutdatedData(i, data->getMediaTime());
	}

	//TODO: Notes:
	//DO WE NEED TO KNOW IF WE ARE ON ERROR STATE? => LOG IT
	//23MS OF DESYNC IS OK => KEEP TRACK OF CURRENT DESYNC
	//AUDIO: BE ABLE TO ASK FOR A LARGER BUFFER ALLOCATOR? => BACK TO THE APP + DYN ALLOCATOR SIZE?
	//VIDEO: HAVE ONLY A FEW DECODED FRAMES: THEY ARRIVE IN ADVANCE ANYWAY
	for (auto i : getInputs()) {
		if(!inputs[i]->getMetadata())
			continue;
		switch (inputs[i]->getMetadata()->getStreamType()) {
		case AUDIO_RAW: {
			Data selectedData;

			while (1) {

				findNearestDataAudio(i, time, selectedData, masterTime);
				if (!selectedData) {
					break;
				}

				auto const audioData = safe_cast<const DataPcm>(selectedData);
				auto data = make_shared<DataBaseRef>(selectedData);
				data->setMediaTime(fractionToClock(Fraction(streams[i].numTicks++ * audioData->getPlaneSize(0) / audioData->getFormat().getBytesPerSample(), audioData->getFormat().sampleRate)));
				log(TR_DEBUG, "Other: send[%s:%s] t=%s (data=%s) (ref=%s)", i, streams[i].data.size(), data->getMediaTime(), data->getMediaTime(), masterTime);
				outputs[i]->emit(data);
				discardStreamOutdatedData(i, data->getMediaTime());
			}
			break;
		}
		case VIDEO_RAW: break;
		default: throw error("unhandled media type (awakeOnFPS)");
		}
	}
}

}
