#include <cstring> // memcpy
#include <memory>
#include <mutex>
#include <algorithm>
#include <thread>
#include "SDL2/SDL.h"

#include "lib_utils/tools.hpp"
#include "lib_utils/system_clock.hpp"
#include "lib_utils/fifo.hpp"
#include "lib_utils/log_sink.hpp"
#include "lib_utils/format.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"

#include "../common/metadata.hpp"
#include "../common/pcm.hpp"
#include "lib_modules/utils/loader.hpp"

#include "render_common.hpp"

static const int64_t TOLERANCE = IClock::Rate / 20;

using namespace Modules;
using namespace Modules::Render;

namespace {

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

struct SDLAudio : ModuleS {
	SDLAudio(IModuleHost* host, IClock* clock)
		: m_host(host),
		  m_clock(clock ? clock : g_SystemClock.get()),
		  m_inputFormat(PcmFormat(44100, AudioLayout::Stereo, AudioSampleFormat::S16, AudioStruct::Interleaved)) {

		if (SDL_InitSubSystem(SDL_INIT_AUDIO) == -1)
			throw std::runtime_error(format("Couldn't initialize: %s", SDL_GetError()));

		if (!reconfigure(m_inputFormat))
			throw error("Audio output creation failed");

		auto input = addInput(this);
		input->setMetadata(make_shared<MetadataRawAudio>());
		auto pushAudio = Signals::BindMember(this, &SDLAudio::push);
		ConnectOutput(m_converter.get(), pushAudio);
	}

	~SDLAudio() {
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	bool reconfigure(PcmFormat inputFormat) {
		if (inputFormat.numPlanes > 1) {
			m_host->log(Warning, "Support for planar audio is buggy. Please set an audio converter.");
			return false;
		}

		SDL_AudioSpec realSpec;
		SDL_AudioSpec audioSpec = toSdlAudioSpec(inputFormat);
		audioSpec.samples = 1024;  /* Good low-latency value for callback */
		audioSpec.callback = &SDLAudio::staticFillAudio;
		audioSpec.userdata = this;

		SDL_CloseAudio();
		if (SDL_OpenAudio(&audioSpec, &realSpec) < 0) {
			m_host->log(Warning, format("Couldn't open audio: %s", SDL_GetError()).c_str());
			return false;
		}

		m_outputFormat = toPcmFormat(realSpec);
		m_converter = loadModule("AudioConvert", m_host, nullptr, &m_outputFormat, -1);
		m_LatencyIn180k = timescaleToClock((uint64_t)realSpec.samples, realSpec.freq);
		m_host->log(Info, format("%s Hz %s ms", realSpec.freq, m_LatencyIn180k * 1000.0f / IClock::Rate).c_str());
		m_inputFormat = inputFormat;
		SDL_PauseAudio(0);
		return true;
	}

	void process(Data data) override {
		m_converter->getInput(0)->push(data);
		m_converter->process();
	}

	void flush() override {
		// wait for everything to be consumed by the audio callback
		for(;;) {
			{
				std::lock_guard<std::mutex> lg(m_protectFifo);
				if(m_fifo.bytesToRead() == 0)
					break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	void push(Data data) {
		auto pcmData = safe_cast<const DataPcm>(data);
		std::lock_guard<std::mutex> lg(m_protectFifo);
		if (m_fifo.bytesToRead() == 0) {
			m_fifoTime = pcmData->getMediaTime() + PREROLL_DELAY;
		}
		for (int i = 0; i < pcmData->getFormat().numPlanes; ++i) {
			m_fifo.write(pcmData->getPlane(i), (size_t)pcmData->getPlaneSize(i));
		}
	}

	void fillAudio(Span buffer) {
		// timestamp of the first sample of the buffer
		auto const bufferTimeIn180k = fractionToClock(m_clock->now()) + m_LatencyIn180k;
		std::lock_guard<std::mutex> lg(m_protectFifo);
		if(m_fifo.bytesToRead() == 0) {
			// consider an empty fifo as being very far in the future:
			// this avoids lots of warning messages in "normal" conditions
			m_fifoTime = std::numeric_limits<int>::max();
		}
		int64_t numSamplesToProduce = buffer.len / m_outputFormat.getBytesPerSample();
		auto const relativeTimePositionIn180k = std::min<int64_t>(m_fifoTime - bufferTimeIn180k, IClock::Rate * 10); // clamp to 10s to avoid integer overflows below
		auto const relativeSamplePosition = relativeTimePositionIn180k * m_outputFormat.sampleRate / int64_t(IClock::Rate);

		if (relativeTimePositionIn180k < -TOLERANCE) {
			auto const numSamplesToDrop = std::min<int64_t>(fifoSamplesToRead(), -relativeSamplePosition);
			m_host->log(Warning, format("must drop fifo data (%s ms)", numSamplesToDrop * 1000.0f / m_outputFormat.sampleRate).c_str());
			fifoConsumeSamples((size_t)numSamplesToDrop);
		} else if (relativeTimePositionIn180k > TOLERANCE) {
			auto const numSilenceSamples = std::min<int64_t>(numSamplesToProduce, relativeSamplePosition);
			m_host->log(Warning, format("insert silence (%s ms)", numSilenceSamples * 1000.0f / m_outputFormat.sampleRate).c_str());
			silenceSamples(buffer, (int)numSilenceSamples);
			numSamplesToProduce -= numSilenceSamples;
		}

		auto const numSamplesToConsume = std::min<int64_t>(numSamplesToProduce, fifoSamplesToRead());
		if (numSamplesToConsume > 0) {
			writeSamples(buffer, m_fifo.readPointer(), (int)numSamplesToConsume);
			fifoConsumeSamples((size_t)numSamplesToConsume);
			numSamplesToProduce -= numSamplesToConsume;
		}

		if (numSamplesToProduce > 0) {
			m_host->log(Warning, "underflow");
			silenceSamples(buffer, (int)numSamplesToProduce);
		}
	}

	static void staticFillAudio(void *udata, uint8_t *stream, int len) {
		auto pThis = (SDLAudio*)udata;
		pThis->fillAudio(Span { stream, (size_t)len });
	}

	uint64_t fifoSamplesToRead() const {
		return m_fifo.bytesToRead() / m_outputFormat.getBytesPerSample();
	}

	void fifoConsumeSamples(size_t n) {
		m_fifo.consume(n * m_outputFormat.getBytesPerSample());
		m_fifoTime += (n * IClock::Rate) / m_outputFormat.sampleRate;
	}

	void writeSamples(Span& dst, uint8_t const* src, int n) {
		assert(n >= 0);
		auto const bytes = (size_t)n * m_outputFormat.getBytesPerSample();
		assert(bytes <= dst.len);
		memcpy(dst.ptr, src, bytes);
		dst += bytes;
	}

	void silenceSamples(Span& dst, int n) {
		assert(n >= 0);
		auto const bytes = (size_t)n * m_outputFormat.getBytesPerSample();
		assert(bytes <= dst.len);
		memset(dst.ptr, 0, bytes);
		dst += bytes;
	}

	IModuleHost* const m_host;
	IClock* const m_clock;

	PcmFormat m_outputFormat {};
	PcmFormat m_inputFormat {};
	std::shared_ptr<IModule> m_converter;
	int64_t m_LatencyIn180k = 0;

	// shared state between:
	// - the producer thread ('push')
	// - the SDL thread ('fillAudio')
	std::mutex m_protectFifo;
	Fifo m_fifo;
	int64_t m_fifoTime;
};

Modules::IModule* createObject(IModuleHost* host, va_list va) {
	auto clock = va_arg(va, IClock*);
	enforce(host, "SDLAudio: host can't be NULL");
	return create<SDLAudio>(host, clock).release();
}

auto const registered = Factory::registerModule("SDLAudio", &createObject);
}
