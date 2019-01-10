#include "audio_convert.hpp"

#include "lib_utils/tools.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/clock.hpp" // clockToTimescale
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"
#include "../common/attributes.hpp"
#include "../common/libav.hpp"
#include "../common/pcm.hpp"

#include <cassert>

extern "C" {
#include <libswresample/swresample.h>
}

using namespace Modules;

namespace {
struct Resampler {
	Resampler() {
		m_SwrContext = swr_alloc();
	}

	~Resampler() {
		swr_free(&m_SwrContext);
	}

	void init() {
		auto const ret = swr_init(m_SwrContext);
		if(ret < 0)
			throw std::runtime_error("Resampler: swr_init failed");
	}

	int convert(uint8_t **out, int out_count, const uint8_t **in, int in_count) {
		auto const ret = swr_convert(m_SwrContext, out, out_count, in, in_count);
		if(ret < 0)
			throw std::runtime_error("Resampler: convert failed");
		return ret;
	}

	int64_t getDelay(int64_t rate) {
		return swr_get_delay(m_SwrContext, rate);
	}

	SwrContext* m_SwrContext;
};

static const char* sampleFormatToString(AudioSampleFormat fmt) {
	switch(fmt) {
	case S16: return "S16";
	case F32: return "F32";
	}
	return "Unknown";
}

static std::string PcmFormatToString(PcmFormat fmt) {
	if(fmt.sampleRate == 0)
		return "[anything]";

	return format("[%s channels, %s kHz, %s]",
	        fmt.numChannels,
	        fmt.sampleRate/1000.0,
	        sampleFormatToString(fmt.sampleFormat));
}

struct AudioConvert : ModuleS {
		/*dstFrameSize is the number of output sample - '-1' is same as input*/
		AudioConvert(KHost* host, PcmFormat dstFormat, int64_t dstLen)
			: m_host(host),
			  m_dstFormat(dstFormat), m_dstLen(dstLen), autoConfigure(true) {
			m_srcFormat = { 0 };
			auto input = addInput(this);
			input->setMetadata(make_shared<MetadataRawAudio>());
			output = addOutput<OutputPcm>();
		}

		AudioConvert(KHost* host, PcmFormat srcFormat, PcmFormat dstFormat, int64_t dstLen)
			:m_host(host),
			 m_srcFormat(srcFormat), m_dstFormat(dstFormat), m_dstLen(dstLen), m_resampler(new Resampler), autoConfigure(false) {
			configure(m_srcFormat);
			auto input = addInput(this);
			input->setMetadata(make_shared<MetadataRawAudio>());
			output = addOutput<OutputPcm>();
		}

		void process(Data data) override {
			auto audioData = safe_cast<const DataPcm>(data);

			bool resyncNeeded = false;

			if (accumulatedTimeInDstSR == -1)
				resyncNeeded = true;

			if (audioData->getFormat() != m_srcFormat) {
				if (!autoConfigure)
					throw error("Incompatible input audio data.");

				resyncNeeded = true;
			}

			auto const srcNumSamples = audioData->size() / audioData->getFormat().getBytesPerSample();
			inputSampleCount += srcNumSamples;

			// detect gaps in input
			{
				auto const expectedInputTime = inputMediaTime + timescaleToClock(inputSampleCount, m_dstFormat.sampleRate);
				auto const actualInputTime = data->getMediaTime();
				if(actualInputTime && std::abs(actualInputTime - expectedInputTime) > 3)
					resyncNeeded = true;
			}

			if(resyncNeeded) {
				reconfigure(audioData->getFormat());
				inputMediaTime = data->getMediaTime();
				inputSampleCount = 0;
				accumulatedTimeInDstSR = 0;
				resyncNeeded = false;
			}

			if (m_dstLen == -1) {
				m_dstLen = divUp((int64_t)srcNumSamples * m_dstFormat.sampleRate, (int64_t)m_srcFormat.sampleRate);
			}

			auto const pSrc = audioData->getPlanes();
			auto const targetNumSamples = m_dstLen - m_outLen;
			bool moreToProcess = doConvert(targetNumSamples, pSrc, srcNumSamples);
			while (moreToProcess) {
				moreToProcess = doConvert(m_dstLen, nullptr, 0);
			}
		}

		void flush() override {
			if (!m_resampler)
				return;

			auto const delay = m_resampler->getDelay(m_dstFormat.sampleRate);
			if (delay == 0 && m_outLen == 0)
				return;

			/*push zeroes to post data until the next m_dstLen boundary*/
			while (doConvert(m_dstLen - m_outLen, nullptr, 0)) {}
			doConvert(0, nullptr, 0);
		}

		/*returns true when more data is available with @targetNumSamples current value*/
		bool doConvert(int targetNumSamples, const void* pSrc, int srcNumSamples) {
			if (!m_out) {
				auto const dstBufferSize = m_dstLen * m_dstFormat.getBytesPerSample();
				m_out = output->getBuffer(0);
				m_out->setFormat(m_dstFormat);
				for (int i = 0; i < m_dstFormat.numPlanes; ++i) {
					m_out->setPlane(i, nullptr, dstBufferSize / m_dstFormat.numPlanes);
				}
			}

			uint8_t* dstPlanes[AUDIO_PCM_PLANES_MAX];
			for (int i=0; i<m_dstFormat.numPlanes; ++i) {
				dstPlanes[i] = m_out->getPlanes()[i] + m_outLen * m_dstFormat.getBytesPerSample() / m_dstFormat.numPlanes;
			}

			const int64_t maxTargetNumSamples = m_out->getPlaneSize(0) * m_dstFormat.numPlanes / m_dstFormat.getBytesPerSample();
			if (targetNumSamples + m_outLen > maxTargetNumSamples) {
				m_host->log(Warning, "Truncating last samples.");
				targetNumSamples = maxTargetNumSamples;
			}
			assert(targetNumSamples >= 0);

			auto const outNumSamples = m_resampler->convert(dstPlanes, targetNumSamples, (const uint8_t**)pSrc, srcNumSamples);
			if (outNumSamples > targetNumSamples)
				throw error(format("Unexpected case: output %s samples when %s was requested (frame size = %s)", outNumSamples, targetNumSamples, m_dstLen));

			m_outLen += outNumSamples;

			if (outNumSamples == targetNumSamples) {
				auto const outPlaneSize = m_dstLen * m_dstFormat.getBytesPerSample() / m_dstFormat.numPlanes;
				for (int i = 0; i < m_dstFormat.numPlanes; ++i) {
					m_out->setPlane(i, m_out->getPlane(i), outPlaneSize);

					/*pad with zeroes on last uncopied framing bytes*/
					auto const outSizeInBytes = m_outLen * m_dstFormat.getBytesPerSample() / m_dstFormat.numPlanes;
					memset(m_out->getPlane(i) + outSizeInBytes, 0, outPlaneSize - outSizeInBytes);
				}

				auto const mediaTime = inputMediaTime + timescaleToClock(accumulatedTimeInDstSR, m_dstFormat.sampleRate);
				m_out->setMediaTime(mediaTime);
				accumulatedTimeInDstSR += m_dstLen;

				output->post(m_out);
				m_out = nullptr;
				m_outLen = 0;

				if (m_resampler->getDelay(m_dstFormat.sampleRate) >= m_dstLen) { //accumulated more than one output buffer: flush.
					return true;
				}
			}

			return false;
		}

		void reconfigure(const PcmFormat &srcFormat) {
			flush();
			m_resampler = make_unique<Resampler>();
			configure(srcFormat);
			m_srcFormat = srcFormat;
		}

		void configure(const PcmFormat &srcFormat) {
			AVSampleFormat avSrcFmt, avDstFmt;
			uint64_t avSrcChannelLayout, avDstChannelLayout;
			int avSrcNumChannels, avDstNumChannels, avSrcSampleRate, avDstSampleRate;
			libavAudioCtxConvertLibav(&srcFormat, avSrcSampleRate, avSrcFmt, avSrcNumChannels, avSrcChannelLayout);
			libavAudioCtxConvertLibav(&m_dstFormat, avDstSampleRate, avDstFmt, avDstNumChannels, avDstChannelLayout);

			swr_alloc_set_opts(m_resampler->m_SwrContext,
			    avDstChannelLayout,
			    avDstFmt,
			    avDstSampleRate,
			    avSrcChannelLayout,
			    avSrcFmt,
			    avSrcSampleRate,
			    0, nullptr);

			m_resampler->init();

			m_host->log(Info, format("Converter configured to: %s -> %s",
			        PcmFormatToString(srcFormat),
			        PcmFormatToString(m_dstFormat)
			    ).c_str());
		}

	private:
		KHost* const m_host;
		PcmFormat m_srcFormat;
		PcmFormat const m_dstFormat;
		int64_t m_dstLen = 0;
		int64_t m_outLen = 0; // number of output samples already in 'm_out'
		std::shared_ptr<DataPcm> m_out;
		std::unique_ptr<Resampler> m_resampler;
		int64_t inputMediaTime = 0;
		int64_t inputSampleCount = 0;
		int64_t accumulatedTimeInDstSR = -1; // '-1' means 'not in sync'
		OutputPcm *output;
		const bool autoConfigure;
};

IModule* createObject(KHost* host, void* va) {
	auto cfg = (AudioConvertConfig*)va;
	enforce(host, "AudioConvert: host can't be NULL");
	enforce(cfg, "AudioConvert: config can't be NULL");

	auto src = cfg->srcFormat;
	auto dst = cfg->dstFormat;
	auto samples = cfg->dstNumSamples;

	enforce(samples == -1 || (samples >= 0 && samples < 1024 * 1024), format("AudioConvert: sample count (%s) must be valid", samples).c_str());
	if(src.sampleRate == 0)
		return Modules::createModule<AudioConvert>(host, dst, samples).release();
	else
		return Modules::createModule<AudioConvert>(host, src, dst, samples).release();
}

auto const registered = Factory::registerModule("AudioConvert", &createObject);
}
