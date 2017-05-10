#pragma once

#include <stdexcept>
#include <lib_utils/log.hpp>
#include <lib_utils/tools.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include "libswscale/swscale.h"
}

namespace ffpp {

class Frame {
	public:
		Frame() {
			avFrame = av_frame_alloc();
			if (!avFrame)
				throw std::runtime_error("Frame allocation failed");
		}
		~Frame() {
			av_frame_free(&avFrame);
		}
		AVFrame* get() {
			return avFrame;
		}

	private:
		AVFrame *avFrame;
};

class Dict {
	public:
		Dict(const std::string &moduleName, const std::string &options)
		: avDict(nullptr), options(options), moduleName(moduleName) {
			buildAVDictionary(options);
			if (av_dict_copy(&avDictOri, avDict, 0) != 0)
				throw(format("[%s] could copy options for \"%s\".", moduleName, options));
		}

		~Dict() {
			ensureAllOptionsConsumed();
			av_dict_free(&avDict);
			av_dict_free(&avDictOri);
		}

		AVDictionaryEntry* get(std::string const name) const {
			return av_dict_get(avDict, name.c_str(), nullptr, 0);
		}

		AVDictionary** operator&() {
			return &avDict;
		}

		void ensureAllOptionsConsumed() const {
			AVDictionaryEntry *avde = nullptr;
			while ( (avde = av_dict_get(*&avDictOri, "", avde, AV_DICT_IGNORE_SUFFIX)) ) {
				if (get(avde->key)) {
					Log::msg(Warning, "codec option \"%s\", value \"%s\" was ignored.", avde->key, avde->value);
				}
			}
		}

	private:
		void buildAVDictionary(const std::string &options) {
			if (av_dict_parse_string(&avDict, options.c_str(), " ", " ", 0) != 0)
				throw(format("[%s] could not parse option list \"%s\".", moduleName, options));

			AVDictionaryEntry *avde = nullptr;
			while ((avde = av_dict_get(*&avDict, "", avde, AV_DICT_IGNORE_SUFFIX))) {
				if (avde->key[0] == '-') {
					for (size_t i=1; i<=strlen(avde->key); ++i) {
						avde->key[i-1] = avde->key[i];
					}
				}
			}
		}

		AVDictionary* avDict, *avDictOri;
		std::string options, moduleName;
};

class SwResampler {
	public:
		SwResampler() {
			m_SwrContext = swr_alloc();
		}

		void setInputLayout(int64_t layout) {
			av_opt_set_int(m_SwrContext, "in_channel_layout", layout, 0);
		}

		void setInputSampleRate(int64_t rate) {
			av_opt_set_int(m_SwrContext, "in_sample_rate", rate, 0);
		}

		void setInputSampleFmt(AVSampleFormat fmt) {
			av_opt_set_sample_fmt(m_SwrContext, "in_sample_fmt", fmt, 0);
		}

		void setOutputLayout(int64_t layout) {
			av_opt_set_int(m_SwrContext, "out_channel_layout", layout, 0);
		}

		void setOutputSampleRate(int64_t rate) {
			av_opt_set_int(m_SwrContext, "out_sample_rate", rate, 0);
		}

		void setOutputSampleFmt(AVSampleFormat fmt) {
			av_opt_set_sample_fmt(m_SwrContext, "out_sample_fmt", fmt, 0);
		}

		void init() {
			auto const ret = swr_init(m_SwrContext);
			if(ret < 0)
				throw std::runtime_error("SwResampler: swr_init failed");
		}

		int convert(uint8_t **out, int out_count, const uint8_t **in , int in_count) {
			auto const ret = swr_convert(m_SwrContext, out, out_count, in, in_count);
			if(ret < 0)
				throw std::runtime_error("SwResampler: convert failed");
			return ret;
		}

		int64_t getDelay(int64_t rate) {
			return swr_get_delay(m_SwrContext, rate);
		}

		~SwResampler() {
			swr_free(&m_SwrContext);
		}

	private:
		SwrContext* m_SwrContext;
};

struct IAvIO {
	virtual ~IAvIO() {}
	virtual AVIOContext* get() = 0;
};

template <typename PrivateData>
class AvIO : public IAvIO {
public:
	AvIO(int (*read)(void *opaque, uint8_t *buf, int buf_size),
		int(*write)(void *opaque, uint8_t *buf, int buf_size),
		int64_t(*seek)(void *opaque, int64_t offset, int whence),
		std::unique_ptr<PrivateData> priv,
		int avioCtxBufferSize = 1024 * 1024, bool isWritable = true)
	: avioCtxBufferSize(avioCtxBufferSize), priv(std::move(priv)) {
		avioCtx = avio_alloc_context((unsigned char*)av_malloc(avioCtxBufferSize), avioCtxBufferSize, isWritable, priv.get(), read, write, seek);
		if (!avioCtx)
			throw std::runtime_error("AvIO allocation failed");
	}
	virtual ~AvIO() {
		if (avioCtx) {
			av_freep(&avioCtx->buffer);
			av_freep(&avioCtx);
		}
	}
	AVIOContext* get() override {
		return avioCtx;
	}

private:
	AVIOContext *avioCtx = nullptr;
	const int avioCtxBufferSize;
	std::unique_ptr<PrivateData> priv;
};

}

