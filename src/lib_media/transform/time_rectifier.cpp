#include "time_rectifier.hpp"
#include "lib_utils/scheduler.hpp"

namespace Modules {

TimeRectifier::TimeRectifier(Fraction frameRate, uint64_t analyzeWindowIn180k)
: frameRate(frameRate), scheduler(new Scheduler(clock)) {
	if (clock->getSpeed() == 0.0) {
		this->analyzeWindowIn180k = std::numeric_limits<uint64_t>::max() / 4;
	} else {
		this->analyzeWindowIn180k = (uint64_t)(analyzeWindowIn180k * clock->getSpeed());
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
	removeOutdatedUnsafe((int64_t)(fractionToClock(clock->now()) - analyzeWindowIn180k));
}

void TimeRectifier::flush() {
	{
		std::unique_lock<std::mutex> lock(inputMutex);
		flushing = true;
		if (clock->getSpeed() == 0.0) {
			this->analyzeWindowIn180k = Clock::Rate / 2;
		}
		flushedCond.wait(lock);
	}
	scheduler = nullptr;
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
						log(Debug, "Remove input[%s][%s] data time media=%s clock=%s", i, removalClockTime, (*data)->getMediaTime(), (*data)->getClockTime());
						data = input[i]->data.erase(data);
						flushedCond.notify_one();
					} else {
						break;
					}
				} else {
					log(Debug, "Remove input[%s][%s] data time media=%s clock=%s", i, removalClockTime, (*data)->getMediaTime(), (*data)->getClockTime());
					data = input[i]->data.erase(data);
				}
			} else {
				data++;
			}
		}
	}
}

void TimeRectifier::awakeOnFPS(Fraction time) {
	{
		std::unique_lock<std::mutex> lock(inputMutex);
		removeOutdatedUnsafe(fractionToClock(time) - analyzeWindowIn180k);
	}

	Data refData;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		if (inputs[i]->getMetadata()->getStreamType() == VIDEO_RAW) {
			{
				std::unique_lock<std::mutex> lock(inputMutex);
				auto dist = std::numeric_limits<int64_t>::max();
				for (auto &currData : input[i]->data) {
					auto const currDist = currData->getClockTime() - fractionToClock(time);
					log(Debug, "Considering data (%s/%s) at time %s (currDist=%s, dist=%s, threshold=%s)", currData->getMediaTime(), currData->getClockTime(), fractionToClock(time), currDist, dist, timescaleToClock(frameRate.den, frameRate.num));
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
			input[i]->currTimeIn180k = fractionToClock(Fraction(numTicks++ * frameRate.den, frameRate.num));
			auto data = shptr(new DataBase(refData));
			data->setMediaTime(input[i]->currTimeIn180k);
			log(Debug, "send[%s:%s] t=%s (data=%s/%s) (ref %s/%s)", i, input[i]->data.size(), input[i]->currTimeIn180k, data->getMediaTime(), data->getClockTime(), refData->getMediaTime(), refData->getClockTime());
			outputs[i]->emit(data);
		}
	}

	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		switch (inputs[i]->getMetadata()->getStreamType()) {
		case SUBTITLE_PKT: case AUDIO_RAW: {
			{
				std::unique_lock<std::mutex> lock(inputMutex);
				//TODO: we are supposed to work sample per sample, but if we keep the packetization then we can operate of compressed streams also
				for (auto &currData : input[i]->data) {
					if (refData->getClockTime() <= currData->getClockTime()) {
						auto data = shptr(new DataBase(currData));
						data->setMediaTime(input[i]->currTimeIn180k);
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

	std::unique_lock<std::mutex> lock(inputMutex);
	removeOutdatedUnsafe(refData->getClockTime());
}

}
