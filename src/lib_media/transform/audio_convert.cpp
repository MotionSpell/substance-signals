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

struct AudioConvert : ModuleS {
		/*dstFrameSize is the number of output sample - '-1' is same as input*/
		AudioConvert(KHost* host, const PcmFormat &dstFormat, int64_t dstNumSamples)
			: m_host(host),
			  dstPcmFormat(dstFormat), m_dstNumSamples(dstNumSamples), autoConfigure(true) {
			srcPcmFormat = { 0 };
			auto input = addInput(this);
			input->setMetadata(make_shared<MetadataRawAudio>());
			output = addOutput<OutputPcm>();
		}

		AudioConvert(KHost* host, const PcmFormat &srcFormat, const PcmFormat &dstFormat, int64_t dstNumSamples)
			:m_host(host),
			 srcPcmFormat(srcFormat), dstPcmFormat(dstFormat), m_dstNumSamples(dstNumSamples), m_resampler(new Resampler), autoConfigure(false) {
			configure(srcPcmFormat);
			auto input = addInput(this);
			input->setMetadata(make_shared<MetadataRawAudio>());
			output = addOutput<OutputPcm>();
		}

		void process(Data data) override {
			auto audioData = safe_cast<const DataPcm>(data);

			if (audioData->getFormat() != srcPcmFormat) {
				if (!autoConfigure)
					throw error("Incompatible input audio data.");

				m_host->log(Info, "Incompatible input audio data. Reconfiguring.");
				reconfigure(audioData->getFormat());
				accumulatedTimeInDstSR = clockToTimescale(data->getMediaTime(), srcPcmFormat.sampleRate);
			}

			auto const srcNumSamples = audioData->size() / audioData->getFormat().getBytesPerSample();
			if (m_dstNumSamples == -1) {
				m_dstNumSamples = divUp((int64_t)srcNumSamples * dstPcmFormat.sampleRate, (int64_t)srcPcmFormat.sampleRate);
			}
			auto const pSrc = audioData->getPlanes();
			auto const targetNumSamples = m_dstNumSamples - m_outSize;
			doConvert(targetNumSamples, pSrc, srcNumSamples);
		}

		void flush() override {
			if (!m_resampler)
				return;

			flushBuffers();
		}

		void flushBuffers() {
			auto const delay = m_resampler->getDelay(dstPcmFormat.sampleRate);
			if (delay == 0 && m_outSize == 0)
				return;

			// we are flushing, these are the last samples
			m_dstNumSamples = std::min(m_dstNumSamples, delay);

			auto const targetNumSamples = m_dstNumSamples;
			m_dstNumSamples += m_outSize;

			doConvert(targetNumSamples, nullptr, 0);
		}

		void doConvert(int targetNumSamples, const void* pSrc, int srcNumSamples) {
			if (!m_out) {
				auto const dstBufferSize = m_dstNumSamples * dstPcmFormat.getBytesPerSample();
				m_out = output->getBuffer(0);
				m_out->setFormat(dstPcmFormat);
				for (int i = 0; i < dstPcmFormat.numPlanes; ++i) {
					m_out->setPlane(i, nullptr, dstBufferSize / dstPcmFormat.numPlanes);
				}
			}

			uint8_t* dstPlanes[AUDIO_PCM_PLANES_MAX];
			for (int i=0; i<dstPcmFormat.numPlanes; ++i) {
				dstPlanes[i] = m_out->getPlanes()[i] + m_outSize * dstPcmFormat.getBytesPerSample() / dstPcmFormat.numPlanes;
			}

			const int64_t maxTargetNumSamples = m_out->getPlaneSize(0) * dstPcmFormat.numPlanes / dstPcmFormat.getBytesPerSample();
			if (targetNumSamples + m_outSize > maxTargetNumSamples) {
				m_host->log(Warning, "Truncating last samples.");
				targetNumSamples = maxTargetNumSamples;
			}
			assert(targetNumSamples >= 0);

			auto const outNumSamples = m_resampler->convert(dstPlanes, targetNumSamples, (const uint8_t**)pSrc, srcNumSamples);
			if (outNumSamples > targetNumSamples)
				throw error(format("Unexpected case: output %s samples when %s was requested (frame size = %s)", outNumSamples, targetNumSamples, m_dstNumSamples));

			m_outSize += outNumSamples;

			if (outNumSamples == targetNumSamples) {
				auto const outPlaneSize = m_dstNumSamples * dstPcmFormat.getBytesPerSample() / dstPcmFormat.numPlanes;
				for (int i = 0; i < dstPcmFormat.numPlanes; ++i)
					m_out->setPlane(i, m_out->getPlane(i), outPlaneSize);

				auto const mediaTime = timescaleToClock(accumulatedTimeInDstSR, dstPcmFormat.sampleRate);
				m_out->setMediaTime(mediaTime);
				accumulatedTimeInDstSR += m_dstNumSamples;

				output->post(m_out);
				m_out = nullptr;
				m_outSize = 0;

				if (m_resampler->getDelay(dstPcmFormat.sampleRate) >= m_dstNumSamples) { //accumulated more than one output buffer: flush.
					flushBuffers();
				}
			}
		}

		void reconfigure(const PcmFormat &srcFormat) {
			flush();
			m_resampler = make_unique<Resampler>();
			configure(srcFormat);
			srcPcmFormat = srcFormat;
		}

		void configure(const PcmFormat &srcFormat) {
			AVSampleFormat avSrcFmt, avDstFmt;
			uint64_t avSrcChannelLayout, avDstChannelLayout;
			int avSrcNumChannels, avDstNumChannels, avSrcSampleRate, avDstSampleRate;
			libavAudioCtxConvertLibav(&srcFormat, avSrcSampleRate, avSrcFmt, avSrcNumChannels, avSrcChannelLayout);
			libavAudioCtxConvertLibav(&dstPcmFormat, avDstSampleRate, avDstFmt, avDstNumChannels, avDstChannelLayout);

			swr_alloc_set_opts(m_resampler->m_SwrContext,
			    avDstChannelLayout,
			    avDstFmt,
			    avDstSampleRate,
			    avSrcChannelLayout,
			    avSrcFmt,
			    avSrcSampleRate,
			    0, nullptr);

			m_resampler->init();
		}

	private:
		KHost* const m_host;
		PcmFormat srcPcmFormat;
		PcmFormat const dstPcmFormat;
		int64_t m_dstNumSamples = 0;
		int64_t m_outSize = 0; // number of output samples already in 'm_out'
		std::shared_ptr<DataPcm> m_out;
		std::unique_ptr<Resampler> m_resampler;
		int64_t accumulatedTimeInDstSR = 0;
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
