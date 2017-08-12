#include "time_rectifier.hpp"
#include "lib_utils/scheduler.hpp"

namespace Modules {

TimeRectifier::TimeRectifier(uint64_t analyzeWindowIn180k, std::unique_ptr<IScheduler> scheduler)
: scheduler(std::move(scheduler)) {
}

void TimeRectifier::process() {
	fillInputQueues();
	mimicOutputs(); //Romain: is it useful to call it twice?
	if (!hasVideo)
		throw error("requires to have one video stream connected");
	removeOutdated();
}

void TimeRectifier::mimicOutputs() {
	auto const numInputs = getNumInputs() - 1;
	auto const numOutputs = outputs.size();
	if (numOutputs < numInputs) {
		for (size_t i = numOutputs; i < numInputs; ++i) {
#if 1 //Romain
			addOutput<OutputDefault>();
			input.push_back(uptr(new Stream));
#else
			//if (!inputs[i]->getMetadata())
			//	throw error(format("No metadata for input %s", i));
			addOutputFromType(inputs[i]->getMetadata()->getStreamType());
			if (!outputs[i]->getMetadata()) {
				log(Warning, "No metadata for output %s: forwarding the input metadata", i);
				outputs[i]->setMetadata(inputs[i]->getMetadata());
			}
#endif
		}
	}
}

void TimeRectifier::fillInputQueues() {
	Data data;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		while (inputs[i]->tryPop(data)) {
			if (data) {
				if (inputs[i]->updateMetadata(data)) {
					outputs[i]->setMetadata(inputs[i]->getMetadata());

					if (inputs[i]->getMetadata()->getStreamType() == VIDEO_RAW) {
						if (hasVideo)
							throw error("only one video stream is allowed");
						hasVideo = true;

						//#278 else if (*safe_cast<MetadataRawVideo>(input[i].metadata)-> == *safe_cast<MetadataRawVideo>(output[i].metadata)->)
						{
							auto const outputMs = 40; //Romain
							scheduler->scheduleEvery(std::bind(&TimeRectifier::awakeOnFPS, this), getUTCInMs(), outputMs);
						}
					}
				}
				input[i]->data.push_back(data);
			}
		}
	}
}

void TimeRectifier::removeOutdated() {
	auto absTimeIn180k = getUTCInMs();
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto data = input[i]->data.begin();
		while (data != input[i]->data.end()) {
			if ((*data)->getClockTime() < absTimeIn180k - analyzeWindowIn180k) {
				if (input[i]->data.size() <= 1) {
					break;
				} else {
					data = input[i]->data.erase(data);
				}
			}
		}
	}
}

void TimeRectifier::awakeOnFPS() {
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		switch (inputs[i]->getMetadata()->getStreamType()) {
		case VIDEO_RAW:
			//TODO: send one frame
			break;
		case AUDIO_RAW:
			//TODO: pull audio for continuity
			break;
		case SUBTITLE_PKT:
			//TODO: if we have data send it, otherwise tick
			break;
		default:
			throw error("unhandled media type (awakeOnFPS)");
		}
	}
}

}
