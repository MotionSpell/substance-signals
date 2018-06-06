#include "sdl_audio.hpp"
#include "render_common.hpp"
#include "lib_utils/tools.hpp"
#include "SDL2/SDL.h"
#include "../common/metadata.hpp"
#include "../transform/audio_convert.hpp"
#include <algorithm>
#include <cstring>

static const int64_t TOLERANCE = IClock::Rate / 20;

namespace Modules {

namespace {

static Signals::ExecutorSync<void(Data)> executorSync;

SDL_AudioSpec toSdlAudioSpec(PcmFormat cfg) {
	SDL_AudioSpec audioSpec {};
	audioSpec.freq = cfg.sampleRate;
	audioSpec.channels = cfg.numChannels;
	switch (cfg.sampleFormat) {
	case S16: audioSpec.format = AUDIO_S16; break;
	case F32: audioSpec.format = AUDIO_F32; break;
	default: throw std::runtime_error("Unknown PcmFormat sample format");
	}
	return audioSpec;
}

PcmFormat toPcmFormat(SDL_AudioSpec audioSpec) {
	auto fmt = PcmFormat(audioSpec.freq, audioSpec.channels);
	fmt.numPlanes = 1;
	switch (audioSpec.format) {
	case AUDIO_S16: fmt.sampleFormat = S16; break;
	case AUDIO_F32: fmt.sampleFormat = F32; break;
	default: throw std::runtime_error("Unknown SDL audio sample format");
	}
	return fmt;
}
}

namespace Render {

bool SDLAudio::reconfigure(PcmFormat inputFormat) {
	if (inputFormat.numPlanes > 1) {
		log(Warning, "Support for planar audio is buggy. Please set an audio converter.");
		return false;
	}

	SDL_AudioSpec realSpec;
	SDL_AudioSpec audioSpec = toSdlAudioSpec(inputFormat);
	audioSpec.samples = 1024;  /* Good low-latency value for callback */
	audioSpec.callback = &SDLAudio::staticFillAudio;
	audioSpec.userdata = this;

	SDL_CloseAudio();
	if (SDL_OpenAudio(&audioSpec, &realSpec) < 0) {
		log(Warning, "Couldn't open audio: %s", SDL_GetError());
		return false;
	}

	m_outputFormat = toPcmFormat(realSpec);
	m_converter = create<Transform::AudioConvert>(m_outputFormat);

	m_LatencyIn180k = timescaleToClock((uint64_t)realSpec.samples, realSpec.freq);
	log(Info, "%s Hz %s ms", realSpec.freq, m_LatencyIn180k * 1000.0f / IClock::Rate);
	m_inputFormat = inputFormat;
	SDL_PauseAudio(0);
	return true;
}

SDLAudio::SDLAudio(const std::shared_ptr<IClock> clock)
	: m_clock(clock), m_inputFormat(PcmFormat(44100, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved)),
	  // consider an empty fifo as being very far in the future:
	  // this avoids lots of warning messages in "normal" conditions
	  fifoTimeIn180k(std::numeric_limits<int>::max()) {

	if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE) == -1)
		throw std::runtime_error(format("Couldn't initialize: %s", SDL_GetError()));

	if (!reconfigure(m_inputFormat))
		throw error("Audio output creation failed");

	auto input = addInput(new Input<DataPcm>(this));
	input->setMetadata(make_shared<MetadataRawAudio>());
	auto pushAudio = Signals::BindMember(this, &SDLAudio::push);
	Signals::Connect(m_converter->getOutput(0)->getSignal(), pushAudio, executorSync);
}

SDLAudio::~SDLAudio() {
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SDLAudio::process(Data data) {
	m_converter->process(data);
}

void SDLAudio::flush() {
	// wait for everything to be consumed by the audio callback
	for(;;) {
		{
			std::lock_guard<std::mutex> lg(m_Mutex);
			if(m_Fifo.bytesToRead() == 0)
				break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void SDLAudio::push(Data data) {
	auto pcmData = safe_cast<const DataPcm>(data);
	std::lock_guard<std::mutex> lg(m_Mutex);
	if (m_Fifo.bytesToRead() == 0) {
		fifoTimeIn180k = pcmData->getMediaTime() + PREROLL_DELAY;
	}
	for (int i = 0; i < pcmData->getFormat().numPlanes; ++i) {
		m_Fifo.write(pcmData->getPlane(i), (size_t)pcmData->getPlaneSize(i));
	}
}

void SDLAudio::fillAudio(uint8_t *stream, int len) {
	// timestamp of the first sample of the buffer
	auto const bufferTimeIn180k = fractionToClock(m_clock->now()) + m_LatencyIn180k;
	std::lock_guard<std::mutex> lg(m_Mutex);
	int64_t numSamplesToProduce = len / m_outputFormat.getBytesPerSample();
	auto const relativeTimePositionIn180k = fifoTimeIn180k - bufferTimeIn180k;
	auto const relativeSamplePosition = relativeTimePositionIn180k * m_outputFormat.sampleRate / int64_t(IClock::Rate);

	if (relativeTimePositionIn180k < -TOLERANCE) {
		auto const numSamplesToDrop = std::min<int64_t>(fifoSamplesToRead(), -relativeSamplePosition);
		log(Warning, "must drop fifo data (%s ms)", numSamplesToDrop * 1000.0f / m_outputFormat.sampleRate);
		fifoConsumeSamples((size_t)numSamplesToDrop);
	} else if (relativeTimePositionIn180k > TOLERANCE) {
		auto const numSilenceSamples = std::min<int64_t>(numSamplesToProduce, relativeSamplePosition);
		log(Warning, "insert silence (%s ms)", numSilenceSamples * 1000.0f / m_outputFormat.sampleRate);
		silenceSamples(stream, (size_t)numSilenceSamples);
		numSamplesToProduce -= numSilenceSamples;
	}

	auto const numSamplesToConsume = std::min<int64_t>(numSamplesToProduce, fifoSamplesToRead());
	if (numSamplesToConsume > 0) {
		writeSamples(stream, m_Fifo.readPointer(), (size_t)numSamplesToConsume);
		fifoConsumeSamples((size_t)numSamplesToConsume);
		numSamplesToProduce -= numSamplesToConsume;
	}

	if (numSamplesToProduce > 0) {
		log(Warning, "underflow");
		silenceSamples(stream, (size_t)numSamplesToProduce);
	}
}

void SDLAudio::staticFillAudio(void *udata, uint8_t *stream, int len) {
	auto pThis = (SDLAudio*)udata;
	pThis->fillAudio(stream, len);
}

uint64_t SDLAudio::fifoSamplesToRead() const {
	return m_Fifo.bytesToRead() / m_outputFormat.getBytesPerSample();
}

void SDLAudio::fifoConsumeSamples(size_t n) {
	m_Fifo.consume(n * m_outputFormat.getBytesPerSample());
	fifoTimeIn180k += (n * IClock::Rate) / m_outputFormat.sampleRate;
}

void SDLAudio::writeSamples(uint8_t*& dst, uint8_t const* src, size_t n) {
	memcpy(dst, src, n * m_outputFormat.getBytesPerSample());
	dst += n * m_outputFormat.getBytesPerSample();
}

void SDLAudio::silenceSamples(uint8_t*& dst, size_t n) {
	memset(dst, 0, n * m_outputFormat.getBytesPerSample());
	dst += n * m_outputFormat.getBytesPerSample();
}

}
}
