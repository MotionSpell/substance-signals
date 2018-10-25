#pragma once

#include <string>
#include <vector>
#include "lib_utils/resolution.hpp"
#include "lib_modules/core/metadata.hpp"

namespace Modules {

//TODO: should be picture and Pcm and return the same fields as MetadataPkt
struct MetadataRawVideo : public IMetadata {
	MetadataRawVideo() : IMetadata(VIDEO_RAW) {
	}
};

struct MetadataRawAudio : public IMetadata {
	MetadataRawAudio() : IMetadata(AUDIO_RAW) {
	}
};

struct MetadataPkt : public IMetadata {
	MetadataPkt(StreamType type) : IMetadata(type) {
	}
	std::string codec; // do not replace this with an enum!
	std::vector<uint8_t> codecSpecificInfo;
};

}
