#include "audio_gap_filler.hpp"
#include "../common/metadata.hpp"

namespace Modules {
namespace Transform {

AudioGapFiller::AudioGapFiller(uint64_t toleranceInFrames)
	: toleranceInFrames(toleranceInFrames) {
	auto input = createInput(this);
	input->setMetadata(make_shared<MetadataRawAudio>());
	output = addOutput<OutputPcm>();
}

void AudioGapFiller::process(Data data) {
	auto audioData = safe_cast<const DataPcm>(data);
	auto const sampleRate = audioData->getFormat().sampleRate;
	auto const timeInSR = clockToTimescale(data->getMediaTime(), sampleRate);
	if (accumulatedTimeInSR == std::numeric_limits<uint64_t>::max()) {
		accumulatedTimeInSR = timeInSR;
	}

	auto const srcNumSamples = audioData->size() / audioData->getFormat().getBytesPerSample();
	auto const diff = (int64_t)(timeInSR - accumulatedTimeInSR);
	if ((uint64_t)std::abs(diff) >= srcNumSamples) {
		if ((uint64_t)std::abs(diff) <= srcNumSamples * (1 + toleranceInFrames)) {
			log(Debug, "Fixing gap of %s samples (input=%s, accumulation=%s)", diff, timeInSR, accumulatedTimeInSR);
			if (diff > 0) {
				auto dataInThePast = make_shared<DataBaseRef>(data);
				dataInThePast->setMediaTime(data->getMediaTime() - timescaleToClock((uint64_t)srcNumSamples, sampleRate));
				process(dataInThePast);
			} else {
				return;
			}
		} else {
			log(Warning, "Discontinuity detected. Reset at time %s (previous: %s).", data->getMediaTime(), timescaleToClock(accumulatedTimeInSR, sampleRate));
			accumulatedTimeInSR = timeInSR;
		}
	}

	auto dataOut = make_shared<DataBaseRef>(data);
	dataOut->setMediaTime(accumulatedTimeInSR, sampleRate);
	output->emit(dataOut);

	accumulatedTimeInSR += srcNumSamples;
}

}
}
