#pragma once

#include "lib_utils/resolution.hpp"
#include "lib_utils/fraction.hpp"
#include "../common/pixel_format.hpp"
#include <string>

enum class VideoCodecType {
	Software,
	Hardware_qsv,
	Hardware_nvenc
};

struct EncoderConfig {
	enum Type {
		Video,
		Audio,
	};

	Type type;
	int bitrate = 128000;
	size_t bufferSize = 10;

	// input format configuration (TODO: deduce it from metadata)
	Resolution res = Resolution(320, 180);
	int sampleRate = 44100;
	int numChannels = 2;

	//video only
	Fraction GOPSize = Fraction(25, 1);
	Fraction frameRate = Fraction(25, 1);
	bool isLowLatency = false;
	VideoCodecType codecType = VideoCodecType::Software;

	std::string avcodecCustom = "";

	// OUTPUT: set by the encoder
	Modules::PixelFormat pixelFormat = Modules::UNKNOWN_PF;
};

