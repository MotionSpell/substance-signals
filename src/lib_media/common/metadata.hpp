#pragma once

#include <string>
#include <vector>
#include "lib_modules/core/metadata.hpp"

namespace Modules {

static const char* StreamTypeNames[] = {
	"audio raw",
	"video raw",
	"audio compressed",
	"video compressed",
	"subtitle compressed",
	"playlist",
	"segment"
};
static_assert(NELEMENTS(StreamTypeNames) == StreamType::SIZE_OF_ENUM_STREAM_TYPE, "StreamType sizes don't match");

struct MetadataFile : IMetadata {
	MetadataFile(const std::string& filename, StreamType streamType, const std::string& mimeType, const std::string& codecName, uint64_t durationIn180k, uint64_t filesize, uint64_t latencyIn180k, bool startsWithRAP, bool EOS)
		: streamType(streamType), filename(filename), mimeType(mimeType), codecName(codecName), durationIn180k(durationIn180k), filesize(filesize), latencyIn180k(latencyIn180k), startsWithRAP(startsWithRAP), EOS(EOS) {
	}
	StreamType getStreamType() const override {
		return streamType;
	}

	union {
		    int resolution[2] = { 0, 0 };
		    int sampleRate;
		};

	StreamType streamType;
	std::string filename, mimeType, codecName/*as per RFC6381*/;
	uint64_t durationIn180k, filesize, latencyIn180k;
	bool startsWithRAP, EOS;
};

//TODO: should be picture and Pcm and return the same fields as MetadataPkt
struct MetadataRawVideo : public IMetadata {
	StreamType getStreamType() const override {
		return VIDEO_RAW;
	}
};

struct MetadataRawAudio : public IMetadata {
	StreamType getStreamType() const override {
		return AUDIO_RAW;
	}
};

struct MetadataPkt : public IMetadata {
	int codecId = -1;
	std::vector<uint8_t> codecSpecificInfo;
};

struct MetadataPktVideo : public MetadataPkt {
	StreamType getStreamType() const override {
		return VIDEO_PKT;
	}
};

struct MetadataPktAudio : public MetadataPkt {
	StreamType getStreamType() const override {
		return AUDIO_PKT;
	}
};

}
