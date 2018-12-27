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
			  dstPcmFormat(dstFormat), dstNumSamples(dstNumSamples), autoConfigure(true) {
			srcPcmFormat = { 0 };
			auto input = addInput(this);
			input->setMetadata(make_shared<MetadataRawAudio>());
			output = addOutput<OutputPcm>();
		}

		AudioConvert(KHost* host, const PcmFormat &srcFormat, const PcmFormat &dstFormat, int64_t dstNumSamples)
			:m_host(host),
			 srcPcmFormat(srcFormat), dstPcmFormat(dstFormat), dstNumSamples(dstNumSamples), m_resampler(new Resampler), autoConfigure(false) {
			configure(srcPcmFormat);
			auto input = addInput(this);
			input->setMetadata(make_shared<MetadataRawAudio>());
			output = addOutput<OutputPcm>();
		}

		void process(Data data) override {
			processBuffer(data);
		}

		void flush() override {
			if (!m_resampler)
				return;

			flushBuffers();
		}

		void flushBuffers() {
			processBuffer(nullptr);
		}

		void processBuffer(Data data) {
			int64_t targetNumSamples;
			uint64_t srcNumSamples = 0;
			uint8_t * const * pSrc = nullptr;

			if (auto audioData = safe_cast<const DataPcm>(data)) {
				if (audioData->getFormat() != srcPcmFormat) {
					if (!autoConfigure)
						throw error("Incompatible input audio data.");

					m_host->log(Info, "Incompatible input audio data. Reconfiguring.");
					reconfigure(audioData->getFormat());
					accumulatedTimeInDstSR = clockToTimescale(data->getMediaTime(), srcPcmFormat.sampleRate);
				}

				srcNumSamples = audioData->size() / audioData->getFormat().getBytesPerSample();
				if (dstNumSamples == -1) {
					dstNumSamples = divUp(srcNumSamples * dstPcmFormat.sampleRate, (uint64_t)srcPcmFormat.sampleRate);
				}
				pSrc = audioData->getPlanes();
				targetNumSamples = dstNumSamples - curDstNumSamples;
			} else {
				auto const delay = m_resampler->getDelay(dstPcmFormat.sampleRate);
				if (delay == 0 && curDstNumSamples == 0) {
					return;
				} else if (delay < dstNumSamples) {
					dstNumSamples = delay; //we are flushing, these are the last samples
				}
				pSrc = nullptr;
				srcNumSamples = 0;
				targetNumSamples = dstNumSamples;
				dstNumSamples += curDstNumSamples;
			}

			std::shared_ptr<DataPcm> out;
			if (curDstNumSamples) {
				out = curOut;
			} else {
				auto const dstBufferSize = dstNumSamples * dstPcmFormat.getBytesPerSample();
				out = output->getBuffer(0);
				out->setFormat(dstPcmFormat);
				for (int i = 0; i < dstPcmFormat.numPlanes; ++i) {
					out->setPlane(i, nullptr, (size_t)dstBufferSize / dstPcmFormat.numPlanes);
				}
			}

			uint8_t* dstPlanes[AUDIO_PCM_PLANES_MAX];
			for (int i=0; i<dstPcmFormat.numPlanes; ++i) {
				dstPlanes[i] = out->getPlanes()[i] + curDstNumSamples * dstPcmFormat.getBytesPerSample() / dstPcmFormat.numPlanes;
			}

			const int64_t maxTargetNumSamples = out->getPlaneSize(0) * dstPcmFormat.numPlanes / dstPcmFormat.getBytesPerSample();
			if (targetNumSamples + curDstNumSamples > maxTargetNumSamples) {
				m_host->log(Warning, "Truncating last samples.");
				targetNumSamples = maxTargetNumSamples;
			}
			assert(targetNumSamples >= 0);

			auto const outNumSamples = m_resampler->convert(dstPlanes, (int)targetNumSamples, (const uint8_t**)pSrc, (int)srcNumSamples);

			if (outNumSamples == targetNumSamples) {
				curDstNumSamples = 0;
				curOut = nullptr;
				targetNumSamples = dstNumSamples;

				auto const outPlaneSize = dstNumSamples * dstPcmFormat.getBytesPerSample() / dstPcmFormat.numPlanes;
				for (int i = 0; i < dstPcmFormat.numPlanes; ++i)
					out->setPlane(i, out->getPlane(i), (size_t)outPlaneSize);

				auto const accumulatedTimeIn180k = timescaleToClock(accumulatedTimeInDstSR, dstPcmFormat.sampleRate);
				out->setMediaTime(accumulatedTimeIn180k);
				accumulatedTimeInDstSR += dstNumSamples;

				output->post(out);
				out = nullptr;
				if (m_resampler->getDelay(dstPcmFormat.sampleRate) >= dstNumSamples) { //accumulated more than one output buffer: flush.
					flushBuffers();
				}
			} else if (outNumSamples < targetNumSamples) {
				curDstNumSamples += outNumSamples;
				curOut = out;
			} else
				throw error(format("Unexpected case: output %s samples when %s was requested (frame size = %s)", outNumSamples, targetNumSamples, dstNumSamples));
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
		int64_t dstNumSamples, curDstNumSamples = 0;
		std::shared_ptr<DataPcm> curOut;
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
