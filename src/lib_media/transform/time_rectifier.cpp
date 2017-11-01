#include "time_rectifier.hpp"
#include "lib_utils/scheduler.hpp"
#include "../common/pcm.hpp"

namespace Modules {

#define TR_DEBUG Debug

static const int64_t ANALYZE_WINDOW_MAX = std::numeric_limits<int64_t>::max() / 2;

TimeRectifier::TimeRectifier(Fraction frameRate, uint64_t analyzeWindowIn180k)
: frameRate(frameRate), scheduler(new Scheduler(clock)) {
	if (clock->getSpeed() == 0.0) {
		this->analyzeWindowIn180k = ANALYZE_WINDOW_MAX;
	} else {
		this->analyzeWindowIn180k = (int64_t)(analyzeWindowIn180k * clock->getSpeed());
	}
}

void TimeRectifier::sanityChecks() {
	if (!hasVideo)
		throw error("requires to have one video stream connected");
}

void TimeRectifier::process() {
	std::unique_lock<std::mutex> lock(inputMutex);
	fillInputQueuesUnsafe();
	sanityChecks();
	removeOutdatedUnsafe((fractionToClock(clock->now()) - analyzeWindowIn180k));
}

void TimeRectifier::flush() {
	std::unique_lock<std::mutex> lock(inputMutex);
	flushing = true;
	auto const finalClockTime = std::max<int64_t>(maxClockTimeIn180k, fractionToClock(clock->now())) + 1;
	log(TR_DEBUG, "Schedule final removal at time %s (max:%s|%s)", finalClockTime, maxClockTimeIn180k, fractionToClock(clock->now()));
	scheduler->scheduleAt([this](Fraction f) {
		log(TR_DEBUG, "Final removal at time %s", fractionToClock(f));
		removeOutdatedUnsafe(fractionToClock(f));
	}, Fraction(finalClockTime, Clock::Rate));
	flushedCond.wait(lock);
}

void TimeRectifier::mimicOutputs() {
	auto const numInputs = getNumInputs() - 1;
	auto const numOutputs = outputs.size();
	if (numOutputs < numInputs) {
		std::unique_lock<std::mutex> lock(inputMutex);
		for (size_t i = numOutputs; i < numInputs; ++i) {
			addOutput<OutputDefault>();
			input.push_back(uptr(new Stream));
		}
	}
}

void TimeRectifier::declareScheduler(Data data, std::unique_ptr<IInput> &input, std::unique_ptr<IOutput> &output) {
	auto const oMeta = output->getMetadata();
	if (!oMeta) {
		log(Debug, "Output isn't connected or doesn't expose a metadata: impossible to check.");
	} else if (input->getMetadata()->getStreamType() != oMeta->getStreamType())
		throw error("Metadata I/O inconsistency");

	if (input->getMetadata()->getStreamType() == VIDEO_RAW) {
		if (hasVideo)
			throw error("Only one video stream is allowed");
		hasVideo = true;

		scheduler->scheduleEvery(std::bind(&TimeRectifier::awakeOnFPS, this, std::placeholders::_1), Fraction(frameRate.den, frameRate.num), 0);
	}
}

void TimeRectifier::fillInputQueuesUnsafe() {
	Data data;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto &currInput = inputs[i];
		while (currInput->tryPop(data)) {
			if (currInput->updateMetadata(data)) {
				declareScheduler(data, currInput, outputs[i]);
			}
			if (data->getClockTime() > maxClockTimeIn180k) {
				maxClockTimeIn180k = data->getClockTime();
			}
			input[i]->data.push_back(data);
		}
	}
}

void TimeRectifier::removeOutdatedUnsafe(int64_t removalClockTime) {
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto data = input[i]->data.begin();
		while (data != input[i]->data.end()) {
			if ((*data)->getClockTime() < removalClockTime) {
				if (input[i]->data.size() <= 1) {
					if (flushing) {
						log(TR_DEBUG, "Remove input[%s] data time media=%s clock=%s (removalClockTime=%s)", i, (*data)->getMediaTime(), (*data)->getClockTime(), removalClockTime);
						data = input[i]->data.erase(data);
						flushedCond.notify_one();
					} else {
						break;
					}
				} else {
					log(TR_DEBUG, "Remove last input[%s] data time media=%s clock=%s (removalClockTime=%s)", i, (*data)->getMediaTime(), (*data)->getClockTime(), removalClockTime);
					data = input[i]->data.erase(data);
				}
			} else {
				data++;
			}
		}
	}
}

void TimeRectifier::awakeOnFPS(Fraction time) {
	std::unique_lock<std::mutex> lock(inputMutex);
	removeOutdatedUnsafe(fractionToClock(time) - analyzeWindowIn180k);

	Data refData;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		if (inputs[i]->getMetadata()->getStreamType() == VIDEO_RAW) {
			{
				auto dist = std::numeric_limits<int64_t>::max();
				for (auto &currData : input[i]->data) {
					auto const currDist = currData->getClockTime() - fractionToClock(time);
					log(Debug, "Video: considering data (%s/%s) at time %s (currDist=%s, dist=%s, threshold=%s)", currData->getMediaTime(), currData->getClockTime(), fractionToClock(time), currDist, dist, timescaleToClock(frameRate.den, frameRate.num));
					if (std::abs(currDist) < dist) {
						/*timings are monotonic so check for a previous data with distance less than one frame*/
						if (currDist <= 0 || (currDist > 0 && dist > timescaleToClock(frameRate.den, frameRate.num))) {
							dist = std::abs(currDist);
							refData = currData;
						}
					}
				}
			}
			if (!refData) {
				log(Warning, "No available reference data for clock time %s", fractionToClock(time));
				return;
			}
			auto data = shptr(new DataBase(refData));
			data->setMediaTime(fractionToClock(Fraction(input[i]->numTicks++ * frameRate.den, frameRate.num)));
			log(TR_DEBUG, "Video: send[%s:%s] t=%s (data=%s/%s) (ref %s/%s)", i, input[i]->data.size(), data->getMediaTime(), data->getMediaTime(), data->getClockTime(), refData->getMediaTime(), refData->getClockTime());
			outputs[i]->emit(data);
		}
	}

	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		switch (inputs[i]->getMetadata()->getStreamType()) {
		case AUDIO_RAW: {
			{
				for (auto &currData : input[i]->data) {
					log(Debug, "Other: considering data (%s/%s) at time %s (ref=%s/%s)", currData->getMediaTime(), currData->getClockTime(), fractionToClock(time), refData->getMediaTime(), refData->getClockTime());
					if (currData->getClockTime() < refData->getClockTime()) {
						//TODO: we are supposed to work sample per sample, but if we keep the packetization then we can operate of compressed streams also
						auto const audioData = safe_cast<const DataPcm>(currData);
						auto data = shptr(new DataBase(currData));
						data->setMediaTime(fractionToClock(Fraction(input[i]->numTicks++ * audioData->getPlaneSize(0) / audioData->getFormat().getBytesPerSample(), audioData->getFormat().sampleRate)));
						log(TR_DEBUG, "Other: send[%s:%s] t=%s (data=%s/%s) (ref %s/%s)", i, input[i]->data.size(), data->getMediaTime(), data->getMediaTime(), data->getClockTime(), refData->getMediaTime(), refData->getClockTime());
						outputs[i]->emit(data); //TODO: multiple packets?
					}
				}
			}
			break;
		}
		case VIDEO_RAW: break;
		default: throw error("unhandled media type (awakeOnFPS)");
		}
	}

	removeOutdatedUnsafe(refData->getClockTime());
}

}
