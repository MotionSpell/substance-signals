#include "audio_gap_filler.hpp"
#include "../common/metadata.hpp"
#include "../common/attributes.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/log_sink.hpp"

namespace Modules {
namespace Transform {

using namespace std;

AudioGapFiller::AudioGapFiller(KHost* host, uint64_t toleranceInFrames)
	: m_host(host), toleranceInFrames(toleranceInFrames) {
	input->setMetadata(make_shared<MetadataRawAudio>());
	output = addOutput();
}

void AudioGapFiller::processOne(Data data) {
	auto audioData = safe_cast<const DataPcm>(data);
	auto const sampleRate = audioData->format.sampleRate;
	auto const timeInSR = clockToTimescale(data->get<PresentationTime>().time, sampleRate);
	if (accumulatedTimeInSR == std::numeric_limits<uint64_t>::max()) {
		accumulatedTimeInSR = timeInSR;
	}

	auto const srcNumSamples = audioData->getSampleCount();
	auto const diff = (int64_t)(timeInSR - accumulatedTimeInSR);
	if (std::abs(diff) >= srcNumSamples) {
		if (std::abs(diff) <= srcNumSamples * (1 + (int64_t)toleranceInFrames)) {
			if (diff > 0) {
				m_host->log(Warning, format("Fixing gap of %s samples (input=%s, accumulation=%s)", diff, timeInSR, accumulatedTimeInSR).c_str());
				auto dataInThePast = clone(data);
				dataInThePast->setMediaTime(data->get<PresentationTime>().time - timescaleToClock((uint64_t)srcNumSamples, sampleRate));
				processOne(dataInThePast);
			} else {
				return; /*small overlap: thrash current sample*/
			}
		} else {
			m_host->log(Warning, format("Discontinuity detected. Reset at time %s (previous: %s).", data->get<PresentationTime>().time, timescaleToClock(accumulatedTimeInSR, sampleRate)).c_str());
			accumulatedTimeInSR = timeInSR;
		}
	}

	auto dataOut = clone(data);
	dataOut->setMediaTime(accumulatedTimeInSR, sampleRate);
	output->post(dataOut);

	accumulatedTimeInSR += srcNumSamples;
}

}
}
