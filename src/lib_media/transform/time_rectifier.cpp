#include "time_rectifier.hpp"
#include "lib_utils/scheduler.hpp"
#include "lib_utils/log_sink.hpp"
#include "lib_utils/format.hpp"
#include "../common/pcm.hpp"
#include <cassert>

static auto const analyzeWindowIn180k = IClock::Rate / 2; // 500 ms

namespace Modules {

#define TR_DEBUG Debug

TimeRectifier::TimeRectifier(KHost* host, std::shared_ptr<IClock> clock_, IScheduler* scheduler_, Fraction frameRate)
	: m_host(host),
	  frameRate(frameRate),
	  threshold(fractionToClock(frameRate.inverse())),
	  clock(clock_),
	  scheduler(scheduler_) {
}

TimeRectifier::~TimeRectifier() {
	std::unique_lock<std::mutex> lock(inputMutex);
	if(m_pendingTaskId)
		scheduler->cancel(m_pendingTaskId);
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
		auto output = addOutput<OutputDefault>();
		streams.push_back(Stream{output, {}});
	}
}

void TimeRectifier::reschedule(Fraction when) {
	m_pendingTaskId = scheduler->scheduleAt(std::bind(&TimeRectifier::onPeriod, this, std::placeholders::_1), when);
}

void TimeRectifier::onPeriod(Fraction timeNow) {
	emitOnePeriod(timeNow);
	{
		std::unique_lock<std::mutex> lock(inputMutex);
		reschedule(timeNow + frameRate.inverse());
	}
}

void TimeRectifier::declareScheduler(IInput* input, IOutput* output) {
	auto const oMeta = output->getMetadata();
	if (!oMeta) {
		m_host->log(Debug, "Output isn't connected or doesn't expose a metadata: impossible to check.");
	} else if (input->getMetadata()->type != oMeta->type)
		throw error("Metadata I/O inconsistency");

	if (input->getMetadata()->type == VIDEO_RAW) {
		if (hasVideo)
			throw error("Only one video stream is allowed");
		hasVideo = true;
		reschedule(clock->now());
	}
}

void TimeRectifier::fillInputQueuesUnsafe() {
	auto now = fractionToClock(clock->now());

	for (auto i : getInputs()) {
		auto &currInput = inputs[i];
		Data data;
		while (currInput->tryPop(data)) {
			streams[i].data.push_back({now, data});
			if (currInput->updateMetadata(data)) {
				declareScheduler(currInput.get(), streams[i].output);
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
			m_host->log(TR_DEBUG, format("Remove last streams[%s] data time media=%s clock=%s (removalClockTime=%s)", inputIdx, (*data).data->getMediaTime(), (*data).creationTime, removalClockTime).c_str());
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
		m_host->log(Debug, format("Video: considering data (%s/%s) at time %s (currDist=%s, dist=%s, threshold=%s)", currData.data->getMediaTime(), currData.creationTime, fractionToClock(time), currDistClock, distClock, threshold).c_str());
		if (std::abs(currDistClock) < distClock) {
			/*timings are monotonic so check for a previous data with distance less than one frame*/
			if (currDistClock <= 0 || (currDistClock > 0 && distClock > threshold)) {
				distClock = std::abs(currDistClock);
				refData = currData.data;
				currDataIdx = idx;
			}
		}
	}
	if ((numTicks > 0) && (stream.data.size() >= 2) && (currDataIdx != 1)) {
		m_host->log(Debug, format("Selected reference data is not contiguous to the last one (index=%s).", currDataIdx).c_str());
		//TODO: pass in error mode: flush all the data where the clock time removeOutdatedAllUnsafe(refData->getCreationTime());
	}
	return refData;
}

Data TimeRectifier::findNearestDataAudio(Stream& stream, int64_t minTime, int64_t maxTime) {

	auto& streamData = stream.data;

	while(!streamData.empty() && streamData.front().data->getMediaTime() <= minTime)
		streamData.erase(streamData.begin());

	if(streamData.empty() || streamData.front().data->getMediaTime() > maxTime)
		return nullptr;

	auto selectedData = streamData.front().data;
	streamData.erase(streamData.begin());
	return selectedData;
}

size_t TimeRectifier::getMasterStreamId() const {
	for(auto i : getInputs()) {
		if (inputs[i]->getMetadata()->type == VIDEO_RAW) {
			return i;
		}
	}
	return 0;
}

// post one "media period" on every output
void TimeRectifier::emitOnePeriod(Fraction time) {
	std::unique_lock<std::mutex> lock(inputMutex);
	discardOutdatedData(fractionToClock(time) - analyzeWindowIn180k);

	// input media time corresponding to the start of the "media period"
	int64_t inMasterTime = 0;

	// output media time corresponding to the start of the "media period"
	int64_t outMasterTime;

	{
		auto const i = getMasterStreamId();
		auto& master = streams[i];
		auto refData = findNearestData(master, time);
		if (!refData) {
			assert(numTicks == 0);

			m_host->log(Warning, format("No available reference data for clock time %s", fractionToClock(time)).c_str());
			return;
		}

		inMasterTime = refData->getMediaTime();

		if (numTicks == 0) {
			m_host->log(Info, format("First available reference clock time: %s", fractionToClock(time)).c_str());
		}

		outMasterTime = fractionToClock(Fraction(numTicks++ * frameRate.den, frameRate.num));
		auto data = make_shared<DataBaseRef>(refData);
		data->setMediaTime(outMasterTime);
		m_host->log(TR_DEBUG, format("Video: send[%s:%s] t=%s (data=%s) (ref %s)", i, master.data.size(), data->getMediaTime(), data->getMediaTime(), refData->getMediaTime()).c_str());
		master.output->post(data);
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
		switch (inputs[i]->getMetadata()->type) {
		case AUDIO_RAW: {

			auto minTime = inMasterTime - threshold;
			auto maxTime = inMasterTime;

			while (auto selectedData = findNearestDataAudio(streams[i], minTime, maxTime)) {
				auto const audioData = safe_cast<const DataPcm>(selectedData);
				auto data = make_shared<DataBaseRef>(selectedData);
				data->setMediaTime(outMasterTime + (selectedData->getMediaTime() - inMasterTime));
				m_host->log(TR_DEBUG, format("Other: send[%s:%s] t=%s (data=%s) (ref=%s)", i, streams[i].data.size(), data->getMediaTime(), data->getMediaTime(), inMasterTime).c_str());
				streams[i].output->post(data);
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
