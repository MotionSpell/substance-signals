#include "audio_gap_filler.hpp"
#include "../common/metadata.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/log_sink.hpp"

namespace Modules {
namespace Transform {

using namespace std;

AudioGapFiller::AudioGapFiller(KHost* host, uint64_t toleranceInFrames)
	: m_host(host), toleranceInFrames(toleranceInFrames) {
	input->setMetadata(make_shared<MetadataRawAudio>());
	output = addOutput<OutputDefault>();
}

void AudioGapFiller::processOne(Data data) {
	auto audioData = safe_cast<const DataPcm>(data);
	auto const sampleRate = audioData->format.sampleRate;
	auto const timeInSR = clockToTimescale(data->getMediaTime(), sampleRate);
	if (accumulatedTimeInSR == std::numeric_limits<uint64_t>::max()) {
		accumulatedTimeInSR = timeInSR;
	}

	auto const srcNumSamples = audioData->data().len / audioData->format.getBytesPerSample();
	auto const diff = (int64_t)(timeInSR - accumulatedTimeInSR);
	if ((uint64_t)std::abs(diff) >= srcNumSamples) {
		if ((uint64_t)std::abs(diff) <= srcNumSamples * (1 + toleranceInFrames)) {
			if (diff > 0) {
				m_host->log(Warning, format("Fixing gap of %s samples (input=%s, accumulation=%s)", diff, timeInSR, accumulatedTimeInSR).c_str());
				auto dataInThePast = make_shared<DataBaseRef>(data);
				dataInThePast->setMediaTime(data->getMediaTime() - timescaleToClock((uint64_t)srcNumSamples, sampleRate));
				processOne(dataInThePast);
			} else {
				return; /*small overlap: thrash current sample*/
			}
		} else {
			m_host->log(Warning, format("Discontinuity detected. Reset at time %s (previous: %s).", data->getMediaTime(), timescaleToClock(accumulatedTimeInSR, sampleRate)).c_str());
			accumulatedTimeInSR = timeInSR;
		}
	}

	auto dataOut = make_shared<DataBaseRef>(data);
	dataOut->setMediaTime(accumulatedTimeInSR, sampleRate);
	output->post(dataOut);

	accumulatedTimeInSR += srcNumSamples;
}

}
}
