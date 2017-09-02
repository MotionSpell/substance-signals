#include "time_rectifier.hpp"
#include "lib_utils/scheduler.hpp"

namespace Modules {

TimeRectifier::TimeRectifier(Fraction frameRate, uint64_t analyzeWindowIn180k, std::unique_ptr<IScheduler> scheduler)
: frameRate(frameRate), analyzeWindowIn180k(analyzeWindowIn180k), scheduler(std::move(scheduler)) {
	//Romain: TODO: when clock speed is 0.0, we should do the same as the current restamper
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
	std::unique_lock<std::mutex> lock(inputMutex);
	flushing = true;
	flushedCond.wait(lock);
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
						log(Warning, "Remove input[%s] data time media=%s clock=%s", i, (*data)->getMediaTime(), (*data)->getClockTime());
						data = input[i]->data.erase(data);
						flushedCond.notify_one();
					} else {
						break;
					}
				} else {
					log(Warning, "Remove input[%s] data time media=%s clock=%s", i, (*data)->getMediaTime(), (*data)->getClockTime());
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
	auto const nowIn180k = fractionToClock(clock->now());
	removeOutdatedUnsafe(nowIn180k - analyzeWindowIn180k);

	Data refData;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		if (inputs[i]->getMetadata()->getStreamType() == VIDEO_RAW) {
			auto dist = std::numeric_limits<int64_t>::max();
			for (auto &currData : input[i]->data) {
				auto const currDist = abs(currData->getClockTime() - fractionToClock(time));
				if (currDist < dist) {
					dist = currDist;
					refData = currData;
				}
			}
			if (!refData) {
				log(Warning, "No available reference data for clock time %s", nowIn180k);
				return;
			}
			input[i]->currTimeIn180k = numTicks++ * fractionToClock(Fraction(frameRate.den, frameRate.num));
			log(Warning, "send %s", input[i]->currTimeIn180k);
			const_cast<DataBase*>(refData.get())->setMediaTime(input[i]->currTimeIn180k); //Romain: this is wrong: we need to point on the same data with data.copy()
			outputs[i]->emit(refData);
		}
	}

	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		switch (inputs[i]->getMetadata()->getStreamType()) {
		case SUBTITLE_PKT: case AUDIO_RAW: {
			//TODO: we are supposed to work sample per sample, but if we keep the packetization then we can operate of compressed streams also
			for (auto &currData : input[i]->data) {
				if (refData->getClockTime() <= currData->getClockTime()) {
					//input[i]->currTimeIn180k = numTicks++ * fractionToClock(Fraction(frameRate.den, frameRate.num));
					const_cast<DataBase*>(currData.get())->setMediaTime(input[i]->currTimeIn180k); //Romain: fractionToClock(time) on multiple packets?
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
