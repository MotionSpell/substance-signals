#pragma once

#include <stdexcept>

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
		Dict(const std::string &moduleName, const std::string &dictName, const std::string &options)
		: avDict(nullptr), options(options), moduleName(moduleName), dictName(dictName) {
			buildAVDictionary(options);
		}

		~Dict() {
			ensureAllOptionsConsumed();
			av_dict_free(&avDict);
		}

		AVDictionaryEntry* get(std::string const name, AVDictionaryEntry* entry = nullptr) const {
			return av_dict_get(avDict, name.c_str(), entry, 0);
		}

		AVDictionary** operator&() {
			return &avDict;
		}

		void ensureAllOptionsConsumed() const {
			auto opt = stringDup(options.c_str());
			char *tok = strtok(opt.data(), "- ");
			while (tok && strtok(nullptr, "- ")) {
				AVDictionaryEntry *avde = nullptr;
				avde = get(tok, avde);
				if (avde) {
					Log::msg(Warning, "codec option \"%s\", value \"%s\" was ignored.", avde->key, avde->value);
				}
				tok = strtok(nullptr, "- ");
			}
		}

	private:
		void set(std::string const& name, std::string const& val) {
			if (av_dict_set(&avDict, name.c_str(), val.c_str(), 0) < 0) {
				Log::msg(Warning, "[%s] unknown %s option \"%s\" with value \"%s\"", moduleName.c_str(), dictName, name, val);
			}
		}

		void buildAVDictionary(const std::string &options) {
			auto opt = stringDup(options.c_str());
			char *tok = strtok(opt.data(), "- ");
			char *tokval = nullptr;
			while (tok && (tokval = strtok(nullptr, "- "))) {
				set(tok, tokval);
				tok = strtok(nullptr, "- ");
			}
		}

		AVDictionary* avDict;
		std::string options, moduleName, dictName;
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

class AvIO {
public:
	AvIO(int avioCtxBufferSize = 1024 * 1024, bool isWritable = true)
	: avioCtxBufferSize(avioCtxBufferSize) {
		int buffer_size = avioCtxBufferSize;
		uint8_t *buffer = (uint8_t*)av_malloc(buffer_size);
		struct buffer_data bd; bd.ptr = buffer; bd.size = buffer_size;
		avioCtx = avio_alloc_context((unsigned char*)av_malloc(avioCtxBufferSize), avioCtxBufferSize, isWritable, &bd, &read_packet, &write_packet, &seek_packet);
		if (!avioCtx)
			throw std::runtime_error("AvIO allocation failed");
	}
	~AvIO() {
		if (avioCtx) {
			av_freep(&avioCtx->buffer);
			av_freep(&avioCtx);
		}
	}
	AVIOContext* get() {
		return avioCtx;
	}

private:
	struct buffer_data {
		uint8_t *ptr;
		int size; ///< size left in the buffer
	};
	static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
		struct buffer_data *bd = (struct buffer_data *)opaque;
		buf_size = (int)FFMIN(buf_size, bd->size);
		memcpy(buf, bd->ptr, buf_size);
		bd->ptr += buf_size;
		bd->size -= buf_size;
		return buf_size;
	}
	static int write_packet(void *opaque, uint8_t *buf, int buf_size) {
		struct buffer_data *bd = (struct buffer_data *)opaque;
		return 0;
	}
	static int64_t seek_packet(void *opaque, int64_t offset, int whence) {
		struct buffer_data *bd = (struct buffer_data *)opaque;
		return 0;
	}

	AVIOContext *avioCtx = nullptr;
	const int avioCtxBufferSize;
};

}

