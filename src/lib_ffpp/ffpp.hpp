#pragma once

#include <stdexcept>
#include <lib_utils/log.hpp>
#include <lib_utils/tools.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>
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
			if (av_dict_parse_string(&avDict, options.c_str(), " ", " ", 0) != 0) {
				Log::msg(Warning, "[%s] could not parse option list \"%s\".", moduleName, options);
			}

			AVDictionaryEntry *avde = nullptr;
			while ((avde = av_dict_get(avDict, "", avde, AV_DICT_IGNORE_SUFFIX))) {
				if (avde->key[0] == '-') {
					for (size_t i=1; i<=strlen(avde->key); ++i) {
						avde->key[i-1] = avde->key[i];
					}
				}
				Log::msg(Debug, "[%s] detected option \"%s\", value \"%s\".", moduleName, avde->key, avde->value);
			}

			if (av_dict_copy(&avDictOri, avDict, 0) != 0)
				throw(format("[%s] could not copy options for \"%s\".", moduleName, options));
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
			while ( (avde = av_dict_get(avDictOri, "", avde, AV_DICT_IGNORE_SUFFIX)) ) {
				if (get(avde->key)) {
					Log::msg(Warning, "codec option \"%s\", value \"%s\" was ignored.", avde->key, avde->value);
				}
			}
		}

	private:
		AVDictionary *avDict = nullptr, *avDictOri = nullptr;
		std::string options, moduleName;
};

}
