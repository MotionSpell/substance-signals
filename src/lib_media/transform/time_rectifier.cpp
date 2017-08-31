#include "time_rectifier.hpp"
#include "lib_utils/scheduler.hpp"

namespace Modules {

TimeRectifier::TimeRectifier(Fraction frameRate, uint64_t analyzeWindowIn180k, std::unique_ptr<IScheduler> scheduler)
: frameRate(frameRate), scheduler(std::move(scheduler)) {
}

void TimeRectifier::sanityChecks() {
	if (!hasVideo)
		throw error("requires to have one video stream connected");
}

void TimeRectifier::process() {
	fillInputQueues();
	sanityChecks();
	removeOutdated();
}

void TimeRectifier::flush() {
	//Romain: //TODO
}

void TimeRectifier::mimicOutputs() {
	auto const numInputs = getNumInputs() - 1;
	auto const numOutputs = outputs.size();
	if (numOutputs < numInputs) {
		for (size_t i = numOutputs; i < numInputs; ++i) {
			addOutput<OutputDefault>();
			input.push_back(uptr(new Stream));
		}
	}
}
void TimeRectifier::declareScheduler(Data data, std::unique_ptr<IInput> &input, std::unique_ptr<IOutput> &output) {
	auto const oMeta = output->getMetadata();
	if (!oMeta) {
		log(Debug, "Output is not connected or doesn't expose a metadata: impossible to check.");
	} else if (input->getMetadata()->getStreamType() != oMeta->getStreamType())
		throw error("Metadata I/O inconsistency");

	if (input->getMetadata()->getStreamType() == VIDEO_RAW) {
		if (hasVideo)
			throw error("Only one video stream is allowed");
		hasVideo = true;

		scheduler->scheduleEvery(std::bind(&TimeRectifier::awakeOnFPS, this, std::placeholders::_1), Fraction(frameRate.den, frameRate.num), clock->now());
	}
}

void TimeRectifier::fillInputQueues() {
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

void TimeRectifier::removeOutdated() {
	auto const absTimeIn180k = fractionToClock(clock->now());
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto data = input[i]->data.begin();
		while (data != input[i]->data.end()) {
			if ((*data)->getClockTime() < (int64_t)(absTimeIn180k - analyzeWindowIn180k)) {
				if (input[i]->data.size() <= 1) {
					break;
				} else {
					data = input[i]->data.erase(data);
				}
			}
		}
	}
}

void TimeRectifier::awakeOnFPS(Fraction time) {
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		switch (inputs[i]->getMetadata()->getStreamType()) {
		case VIDEO_RAW:
			//const_cast<DataBase*>(data.get())->setMediaTime(restampedTime); //Romain: don't do that: make the difference between the metadata and the data + add data.copy()??
			break;
		case AUDIO_RAW:
			//TODO: pull audio for continuity => send audio until time
			//Romain: we are supposed to work sample per sample, but if we keep the packetization then we can operate of compressed streams also
			//Romain: at the same time we should output exactly 40ms of data...
			break;
		case SUBTITLE_PKT:
			//TODO: if we have data send it, otherwise tick = sparse stream => Romain: what about outputing 40ms only?
			break;
		default:
			throw error("unhandled media type (awakeOnFPS)");
		}
	}
}

}
