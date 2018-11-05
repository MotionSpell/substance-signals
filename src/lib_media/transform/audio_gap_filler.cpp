#include "audio_gap_filler.hpp"
#include "../common/metadata.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/log_sink.hpp"

namespace Modules {
namespace Transform {

AudioGapFiller::AudioGapFiller(IModuleHost* host, uint64_t toleranceInFrames)
	: m_host(host), toleranceInFrames(toleranceInFrames) {
	auto input = addInput(this);
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
			m_host->log(Debug, format("Fixing gap of %s samples (input=%s, accumulation=%s)", diff, timeInSR, accumulatedTimeInSR).c_str());
			if (diff > 0) {
				auto dataInThePast = make_shared<DataBaseRef>(data);
				dataInThePast->setMediaTime(data->getMediaTime() - timescaleToClock((uint64_t)srcNumSamples, sampleRate));
				process(dataInThePast);
			} else {
				return;
			}
		} else {
			m_host->log(Warning, format("Discontinuity detected. Reset at time %s (previous: %s).", data->getMediaTime(), timescaleToClock(accumulatedTimeInSR, sampleRate)).c_str());
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
