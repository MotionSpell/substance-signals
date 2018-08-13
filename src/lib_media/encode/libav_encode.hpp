#pragma once

#include "../common/pcm.hpp"
#include "../common/picture.hpp"
#include <string>

struct EncoderConfig {
	enum Type {
		Video,
		Audio,
	};

	Type type;
	int bitrate = 128000;
	size_t bufferSize = Modules::ALLOC_NUM_BLOCKS_DEFAULT;

	//video only
	Resolution res = Resolution(320, 180);
	Fraction GOPSize = Fraction(25, 1);
	Fraction frameRate = Fraction(25, 1);
	bool isLowLatency = false;
	Modules::VideoCodecType codecType = Modules::Software;
	Modules::PixelFormat pixelFormat = Modules::UNKNOWN_PF; //set by the encoder

	//audio only
	int sampleRate = 44100;
	int numChannels = 2;

	std::string avcodecCustom = "";
};

