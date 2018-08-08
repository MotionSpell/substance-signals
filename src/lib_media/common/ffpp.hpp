// Copyright 2013 - Romain Bouqueau, Samurai Akihiro, Motion Spell S.A.R.L.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <stdexcept>
#include <lib_utils/log.hpp>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/dict.h>
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
