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
	  framePeriod(frameRate.inverse()),
	  threshold(fractionToClock(framePeriod)),
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
	if(!m_started) {
		reschedule(clock->now());
		m_started = true;
	}
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
	m_pendingTaskId = {};
	emitOnePeriod(timeNow);
	{
		std::unique_lock<std::mutex> lock(inputMutex);
		reschedule(timeNow + framePeriod);
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

Data TimeRectifier::chooseNextMasterFrame(Stream& stream) {
	if(stream.data.empty())
		return stream.blank;

	auto r = stream.data.front().data;
	stream.data.erase(stream.data.begin());

	stream.blank = r;
	return r;
}

int TimeRectifier::getMasterStreamId() const {
	for(auto i : getInputs()) {
		if (inputs[i]->getMetadata() && inputs[i]->getMetadata()->type == VIDEO_RAW) {
			return i;
		}
	}
	return -1;
}

// post one "media period" on all outputs.
//
// In this function we distinguish between "in" media times, and "out" media times.
//
// - "in" media times are the media times of the input samples.
// They might contain gaps, offsets, etc.
// Thus, they should only be used for synchronisation between input streams.
//
// - "out" media times are the media times of the output samples.
// They must be perfectly continuous, increasing, and must not depend in any way
// of the input framing.
void TimeRectifier::emitOnePeriod(Fraction now) {
	std::unique_lock<std::mutex> lock(inputMutex);
	fillInputQueuesUnsafe();
	sanityChecks();
	discardOutdatedData(fractionToClock(now) - analyzeWindowIn180k);

	// output media times corresponding to the "media period"
	auto const outMasterTime = Interval {
		fractionToClock(Fraction((numTicks+0) * framePeriod.num, framePeriod.den)),
		fractionToClock(Fraction((numTicks+1) * framePeriod.num, framePeriod.den))
	};

	// input media times corresponding to the "media period"
	Interval inMasterTime {};

	{
		auto const i = getMasterStreamId();
		if(i == -1)
			return; // no master stream yet
		auto& master = streams[i];
		auto masterFrame = chooseNextMasterFrame(master);
		if (!masterFrame) {
			assert(numTicks == 0);

			m_host->log(Warning, format("No available reference data for clock time %s", fractionToClock(now)).c_str());
			return;
		}

		inMasterTime.start = masterFrame->getMediaTime();
		inMasterTime.stop = inMasterTime.start + (outMasterTime.stop - outMasterTime.start);

		if (numTicks == 0) {
			m_host->log(Info, format("First available reference clock time: %s", fractionToClock(now)).c_str());
		}

		auto data = make_shared<DataBaseRef>(masterFrame);
		data->setMediaTime(outMasterTime.start);
		m_host->log(TR_DEBUG, format("Video: send[%s:%s] t=%s (data=%s) (ref %s)", i, master.data.size(), data->getMediaTime(), data->getMediaTime(), masterFrame->getMediaTime()).c_str());
		master.output->post(data);
		discardStreamOutdatedData(i, data->getMediaTime());
	}

	//TODO: Notes:
	//DO WE NEED TO KNOW IF WE ARE ON ERROR STATE? => LOG IT
	//23MS OF DESYNC IS OK => KEEP TRACK OF CURRENT DESYNC
	//AUDIO: BE ABLE TO ASK FOR A LARGER BUFFER ALLOCATOR? => BACK TO THE APP + DYN ALLOCATOR SIZE?
	//VIDEO: HAVE ONLY A FEW DECODED FRAMES: THEY ARRIVE IN ADVANCE ANYWAY
	for (auto i : getInputs()) {
		auto& input = inputs[i];

		if(!input->getMetadata())
			continue;

		switch (input->getMetadata()->type) {
		case AUDIO_RAW:
			emitOnePeriod_RawAudio(i, inMasterTime, outMasterTime);
			break;
		case VIDEO_RAW:
			break;
		default: throw error("unhandled media type (awakeOnFPS)");
		}
	}

	++numTicks;
}

static void resizePcm(DataPcm* pcm, int sampleCount) {
	for(int i=0; i < pcm->getFormat().numPlanes; ++i)
		pcm->setPlane(i, nullptr, sampleCount * pcm->getFormat().getBytesPerSample());
}

void TimeRectifier::emitOnePeriod_RawAudio(int i, Interval inMasterTime, Interval outMasterTime) {
	auto& stream = streams[i];

	if(stream.fmt.sampleRate == 0) {
		if(stream.data.empty())
			return;
		stream.fmt = safe_cast<const DataPcm>(stream.data.front().data)->getFormat();
	}

	auto const BPS = stream.fmt.getBytesPerSample();

	// convert a timestamp to an absolute sample count
	auto toSamples = [&](int64_t time) -> int64_t {
		return (time * stream.fmt.sampleRate) / IClock::Rate;
	};

	auto toSamplesP = [&](Interval p) -> Interval {
		return { toSamples(p.start), toSamples(p.stop) };
	};

	auto getSampleInterval = [&](const DataPcm* pcm) -> Interval {
		auto const start = toSamples(pcm->getMediaTime());
		return Interval {
			start,
			start + int64_t(pcm->getPlaneSize(0) / BPS)
		};
	};

	// convert all times to absolute sample counts.
	// This way we handle early all precision issues.
	auto const outMasterSamples = toSamplesP(outMasterTime);
	auto const inMasterSamples = toSamplesP(inMasterTime);

	// Create an output sample. We want it to start at 'outMasterTime.start',
	// and to cover the full 'outMasterTime' interval.
	auto pcm = stream.output->getBuffer<DataPcm>(0);
	pcm->setMediaTime(outMasterTime.start);
	pcm->setFormat(stream.fmt);
	resizePcm(pcm.get(), outMasterSamples.stop - outMasterSamples.start);

	// remove obsolete samples
	while(!stream.data.empty()) {
		auto const inputData = safe_cast<const DataPcm>(stream.data.front().data);
		auto const inSamples = getSampleInterval(inputData.get());
		if(inSamples.stop >= inMasterSamples.start)
			break;
		stream.data.erase(stream.data.begin());
	}

	// fill the period "outMasterSamples" with portions of input audio samples
	// that intersect with the "media period".
	for(auto& data : stream.data) {
		auto const inputData = safe_cast<const DataPcm>(data.data);

		auto const inSamples = getSampleInterval(inputData.get());

		// intersect period of this data with the "media period"
		auto const left = std::max(inSamples.start, inMasterSamples.start);
		auto const right = std::min(inSamples.stop, inMasterSamples.stop);

		// intersection is empty
		if(left >= right)
			continue;

		for(int i=0; i < stream.fmt.numPlanes; ++i) {
			auto src = inputData->getPlane(i) + (left - inSamples.start) * BPS;
			auto dst = pcm->getPlane(i) + (left - inMasterSamples.start) * BPS;
			memcpy(dst, src, (right - left) * BPS);
		}
	}

	stream.output->post(pcm);

	m_host->log(TR_DEBUG, format("Other: send[%s:%s] t=%s (data=%s) (ref=%s)", i, stream.data.size(), pcm->getMediaTime(), pcm->getMediaTime(), inMasterTime.start).c_str());
}

}
