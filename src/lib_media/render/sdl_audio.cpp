#include "sdl_audio.hpp"
#include "render_common.hpp"
#include "lib_utils/tools.hpp"
#include "SDL2/SDL.h"
#include "../common/metadata.hpp"
#include "../transform/audio_convert.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>

static const int64_t TOLERANCE = IClock::Rate / 20;

namespace Modules {

namespace {

static Signals::ExecutorSync<void(Data)> executorSync;

SDL_AudioSpec SDLAudioSpecConvert(const PcmFormat *cfg) {
	SDL_AudioSpec audioSpec {};
	audioSpec.freq = cfg->sampleRate;
	audioSpec.channels = cfg->numChannels;
	switch (cfg->sampleFormat) {
	case S16: audioSpec.format = AUDIO_S16; break;
	case F32: audioSpec.format = AUDIO_F32; break;
	default: throw std::runtime_error("Unknown SDL audio format");
	}

	return audioSpec;
}
}

namespace Render {

bool SDLAudio::reconfigure(PcmFormat const * const pcmData) {
	if (pcmData->numPlanes > 1) {
		log(Warning, "Support for planar audio is buggy. Please set an audio converter.");
		return false;
	}

	SDL_AudioSpec realSpec;
	SDL_AudioSpec audioSpec = SDLAudioSpecConvert(pcmData);
	audioSpec.samples = 1024;  /* Good low-latency value for callback */
	audioSpec.callback = &SDLAudio::staticFillAudio;
	audioSpec.userdata = this;
	bytesPerSample = pcmData->getBytesPerSample();

	SDL_CloseAudio();
	if (SDL_OpenAudio(&audioSpec, &realSpec) < 0) {
		log(Warning, "Couldn't open audio: %s", SDL_GetError());
		return false;
	}

	if(realSpec.format != audioSpec.format) {
		log(Error, "Unsupported audio sample format: %s", realSpec.format);
		return false;
	}

	m_LatencyIn180k = timescaleToClock((uint64_t)realSpec.samples, realSpec.freq);
	log(Info, "%s Hz %s ms", realSpec.freq, m_LatencyIn180k * 1000.0f / IClock::Rate);
	pcmFormat = make_unique<PcmFormat>((*pcmData));
	SDL_PauseAudio(0);
	return true;
}

SDLAudio::SDLAudio(const std::shared_ptr<IClock> clock)
	: m_clock(clock), pcmFormat(new PcmFormat(44100, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved)),
	  m_converter(create<Transform::AudioConvert>(*pcmFormat)), fifoTimeIn180k(0) {

	if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE) == -1)
		throw std::runtime_error(format("Couldn't initialize: %s", SDL_GetError()));

	if (!reconfigure(pcmFormat.get()))
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
	int64_t numSamplesToProduce = len / bytesPerSample;
	auto const relativeTimePositionIn180k = fifoTimeIn180k - bufferTimeIn180k;
	auto const relativeSamplePosition = relativeTimePositionIn180k * pcmFormat->sampleRate / int64_t(IClock::Rate);

	if (relativeTimePositionIn180k < -TOLERANCE) {
		auto const numSamplesToDrop = std::min<int64_t>(fifoSamplesToRead(), -relativeSamplePosition);
		log(Warning, "must drop fifo data (%s ms)", numSamplesToDrop * 1000.0f / pcmFormat->sampleRate);
		fifoConsumeSamples((size_t)numSamplesToDrop);
	} else if (relativeTimePositionIn180k > TOLERANCE) {
		auto const numSilenceSamples = std::min<int64_t>(numSamplesToProduce, relativeSamplePosition);
		log(Warning, "insert silence (%s ms)", numSilenceSamples * 1000.0f / pcmFormat->sampleRate);
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
	return m_Fifo.bytesToRead() / bytesPerSample;
}

void SDLAudio::fifoConsumeSamples(size_t n) {
	m_Fifo.consume(n * bytesPerSample);
	fifoTimeIn180k += (n * IClock::Rate) / pcmFormat->sampleRate;
}

void SDLAudio::writeSamples(uint8_t*& dst, uint8_t const* src, size_t n) {
	memcpy(dst, src, n * bytesPerSample);
	dst += n * bytesPerSample;
}

void SDLAudio::silenceSamples(uint8_t*& dst, size_t n) {
	memset(dst, 0, n * bytesPerSample);
	dst += n * bytesPerSample;
}

}
}
