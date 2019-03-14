#pragma once

#include <string>
#include "lib_media/common/resolution.hpp"
#include "lib_modules/core/metadata.hpp"

namespace Modules {

struct MetadataFile : IMetadata {
	MetadataFile(StreamType type_)
		: IMetadata(type_) {
	}

	MetadataFile(const MetadataFile& other) :
		IMetadata(other.type) {
		resolution     = other.resolution;
		sampleRate     = other.sampleRate;
		filename       = other.filename;
		mimeType       = other.mimeType;
		codecName      = other.codecName;
		durationIn180k = other.durationIn180k;
		filesize       = other.filesize;
		latencyIn180k  = other.latencyIn180k;
		startsWithRAP  = other.startsWithRAP;
		EOS            = other.EOS;
	}

	Resolution resolution;
	int sampleRate;

	std::string filename {};
	std::string mimeType {};
	std::string codecName {}; /*as per RFC6381*/
	uint64_t durationIn180k = 0;
	uint64_t filesize = 0;
	uint64_t latencyIn180k = 1;
	bool startsWithRAP = false;
	bool EOS = true;
};

}

