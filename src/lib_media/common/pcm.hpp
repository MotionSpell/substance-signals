#pragma once

#include <cstring> // memcpy
#include <stdexcept>
#include <vector>
#include "lib_modules/core/buffer.hpp"
#include "lib_modules/core/database.hpp"
#include "lib_modules/core/raw_buffer.hpp"

namespace Modules {

enum AudioSampleFormat {
	S16,
	F32
};

enum AudioLayout {
	Mono,
	Stereo,
	FivePointOne,
};

enum AudioStruct {
	Interleaved,
	Planar
};

static const int AUDIO_PCM_PLANES_MAX = 8;
}

namespace {
uint8_t getNumChannelsFromLayout(Modules::AudioLayout layout) {
	switch (layout) {
	case Modules::Mono: return 1;
	case Modules::Stereo: return 2;
	case Modules::FivePointOne: return 6;
	default: throw std::runtime_error("Unknown audio layout");
	}
}
}

namespace Modules {

class PcmFormat {
	public:
		PcmFormat(int sampleRate = 44100, int numChannels = 2,
		    AudioLayout layout = Stereo, AudioSampleFormat sampleFormat = F32, AudioStruct structa = Planar) :
			sampleRate(sampleRate), numChannels(numChannels), layout(layout), sampleFormat(sampleFormat), numPlanes((structa == Planar) ? numChannels : 1) {
		}

		PcmFormat(int sampleRate, AudioLayout layout, AudioSampleFormat sampleFormat, AudioStruct structa) :
			sampleRate(sampleRate), numChannels(getNumChannelsFromLayout(layout)), layout(layout), sampleFormat(sampleFormat), numPlanes((structa == Planar) ? numChannels : 1) {
		}

		bool operator!=(const PcmFormat& other) const {
			return !(*this == other);
		}

		bool operator==(const PcmFormat& other) const {
			if (other.sampleRate != sampleRate)
				return false;

			if (other.numChannels != numChannels)
				return false;

			if (other.layout != layout)
				return false;

			if (other.sampleFormat != sampleFormat)
				return false;

			if (other.numPlanes != numPlanes)
				return false;

			return true;
		}

		int getBytesPerSample() const {
			int b = 1;
			switch (sampleFormat) {
			case S16: b *= 2; break;
			case F32: b *= 4; break;
			default: throw std::runtime_error("Unknown audio format");
			}
			b *= getNumChannelsFromLayout(layout);
			return b;
		}

		int sampleRate;
		int numChannels;
		AudioLayout layout;

		AudioSampleFormat sampleFormat;
		int numPlanes;
};

class DataPcm : public DataBase {
	public:
		DataPcm(size_t size) {
			if (size > 0)
				throw std::runtime_error("Forbidden operation. Requested size must be 0. Then call setFormat().");
			buffer = std::make_shared<RawBuffer>(size);
		}

		PcmFormat format;

		uint8_t* getPlane(int planeIdx) const {
			if (planeIdx > format.numPlanes)
				throw std::runtime_error("Pcm plane doesn't exist.");
			return const_cast<uint8_t*>(buffer->data().ptr + getPlaneSize() * planeIdx);
		}

		uint64_t getPlaneSize() const {
			return getSampleCount() * format.getBytesPerSample() / format.numPlanes;
		}

		void setSampleCount(int sampleCount) {
			buffer->resize(sampleCount * format.getBytesPerSample());
		}

		int getSampleCount() const {
			return buffer->data().len / format.getBytesPerSample();
		}
};

}

